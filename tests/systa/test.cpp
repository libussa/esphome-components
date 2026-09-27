#include "systa_reader.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace esphome;
using namespace esphome::systa_reader;
std::vector<uint8_t> checked(std::vector<uint8_t> f) {
  unsigned sum=0;for(size_t i=0;i+1<f.size();i++)sum+=f[i];
  f.back()=uint8_t(-sum);return f;
}
std::vector<uint8_t> sensor_frame() {
  std::vector<uint8_t> f(38,0);f[0]=0xfc;f[1]=35;f[2]=0x0c;f[3]=1;
  f[8]=0;f[9]=215;return checked(f);
}
std::vector<uint8_t> display(const std::string &text) {
  std::vector<uint8_t> f(37,' ');f[0]=0x0f;f[1]=0x22;f[2]=4;f[3]=0;
  std::copy(text.begin(),text.end(),f.begin()+4);return checked(f);
}
int main() {
  SystaReader reader;sensor::Sensor temperature;text_sensor::TextSensor screen,aqua;
  reader.set_enabled_mask(SystaReader::DEV_PALLETTI_II);
  reader.set_palletti_ii_ta_sensor(&temperature);
  reader.set_palletti_ii_display_text_sensor(&screen);
  reader.set_aqua_display_text_sensor(&aqua);
  reader.setup();
  auto good=sensor_frame();auto bad=good;bad[9]^=1;
  // Corrupt frames must not publish or prevent the subsequent valid frame.
  reader.feed(bad);reader.feed(good);reader.loop();
  assert(temperature.history.size()==1);assert(std::abs(temperature.state-21.5f)<0.01f);
  // Logging configuration must never determine data acceptance.
  reader.set_log_invalid(false);reader.feed(bad);reader.loop();
  assert(temperature.history.size()==1);
  // Stable temperatures still publish on every valid frame: timeout filters
  // must track bus traffic, not only numerical changes.
  reader.feed(good);reader.loop();assert(temperature.history.size()==2);
  auto first=display("Initial");reader.feed(first);reader.loop();
  assert(screen.history.size()==1);assert(aqua.history.empty());
  reader.feed(first);reader.loop();assert(screen.history.size()==1);
  // A final change inside the old five-minute window must be delivered,
  // even if no later display frame arrives.
  reader.feed(display("Final"));reader.loop();
  assert(screen.history.size()==2);assert(screen.state.substr(0,5)=="Final");
  auto corrupt_display=display("Corrupt");corrupt_display[10]^=1;
  reader.feed(corrupt_display);reader.loop();assert(screen.history.size()==2);

  SystaReader comfort;
  sensor::Sensor hours, starts, target, mode;
  binary_sensor::BinarySensor burner, lon;
  text_sensor::TextSensor error, version;
  comfort.set_enabled_mask(SystaReader::DEV_COMFORT);
  comfort.set_comfort_bst_sensor(&hours);
  comfort.set_comfort_kst_sensor(&starts);
  comfort.set_comfort_tw_s_sensor(&target);
  comfort.set_comfort_ba1_sensor(&mode);
  comfort.set_comfort_stat_b1_binary_sensor(&burner);
  comfort.set_comfort_stat_lon_binary_sensor(&lon);
  comfort.set_comfort_boiler_err_text_sensor(&error);
  comfort.set_comfort_fw_version_text_sensor(&version);
  comfort.setup();
  auto from_hex = [](const std::string &hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.size(); i += 2)
      bytes.push_back(std::stoul(hex.substr(i, 2), nullptr, 16));
    return bytes;
  };
  const auto captured = from_hex(
      "FC230C0200000000000000000000000008000000029600001B76FFFF00070007000000000096");
  // Every checksum-valid short variant must be rejected without publishing.
  for (size_t n = 0; n < 32; ++n) {
    std::vector<uint8_t> short_frame(captured.begin(), captured.begin() + 4 + n);
    short_frame.push_back(0);
    short_frame[1] = n + 2;
    comfort.feed(checked(short_frame));
    comfort.loop();
  }
  assert(hours.history.empty() && error.history.empty() && lon.history.empty());
  comfort.feed(captured); comfort.loop();
  assert(hours.state == 662 && starts.state == 7030);
  assert(target.state == 0 && mode.state == 7);
  assert(!burner.state && lon.state && error.state == "0xFFFF");
  comfort.feed(captured); comfort.loop();
  assert(hours.history.size() == 2 && burner.history.size() == 2);
  auto broken = captured; broken[20] ^= 1;
  comfort.feed(broken); comfort.loop();
  assert(hours.history.size() == 2);
  auto active = captured;
  active[16] = 0x02; active[17] = 0x05; // burner, boiler and HK1 pumps
  active[18] = 0x80; // counter above signed int range
  comfort.feed(checked(active)); comfort.loop();
  assert(burner.state && !lon.state && hours.state > 2147483647.0);
  comfort.feed(from_hex("FD05AA0C02040240")); comfort.loop();
  assert(version.state == "2.4.2");

  sensor::Sensor unused_supply, unused_bottom;
  reader.set_palletti_ii_hk2_tv2_sensor(&unused_supply);
  reader.set_palletti_ii_tpu_sensor(&unused_bottom);
  auto probes = from_hex(
      "FC270C012709431700C302B400D20000011C000000DFFEDB00DDFEDB0000FEDDFEDD0000002626042743");
  reader.feed(probes); reader.loop();
  assert(std::isnan(unused_supply.state) && std::isnan(unused_bottom.state));
  probes[8] = 0xFF; probes[9] = 0x9C; // real -10 C outside
  probes[22] = 0x00; probes[23] = 0xFA; // connected 25 C probe
  reader.feed(checked(probes)); reader.loop();
  assert(temperature.state == -10 && unused_supply.state == 25);
  std::cout << "PASS: captured Comfort/Pelletti data, all short status frames, "
               "freshness publications, unsigned counters and absent probes\n";
  std::cout<<"PASS: corrupt frames rejected, parser recovers, correct display publisher, changed/duplicate/final display handling\n";
}
