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
  std::cout<<"PASS: corrupt frames rejected, parser recovers, correct display publisher, changed/duplicate/final display handling\n";
}
