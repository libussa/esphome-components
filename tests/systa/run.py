#!/usr/bin/env python3
"""Compile real vendored parser/decoders against minimal host ESPHome doubles."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='systa-test-') as td:
 p=Path(td)
 def put(name,txt):
  f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(txt)
 put('esphome/core/component.h','''#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <algorithm>
namespace esphome {
inline uint32_t millis() { return 1000; }
namespace setup_priority { constexpr float DATA=600; }
class Component { public: virtual void setup(){}; virtual void loop(){}; virtual float get_setup_priority() const {return 0;} };
}
''')
 put('esphome/core/log.h','''#pragma once
#define ESPHOME_LOG_LEVEL 0
#define ESPHOME_LOG_LEVEL_VERBOSE 5
#define ESP_LOGE(...) (void)0
#define ESP_LOGW(...) (void)0
#define ESP_LOGI(...) (void)0
#define ESP_LOGD(...) (void)0
#define ESP_LOGV(...) (void)0
#define ESP_LOGVV(...) (void)0
''')
 put('esphome/components/uart/uart.h','''#pragma once
#include <cstdint>
#include <deque>
#include <vector>
namespace esphome { namespace uart {
class UARTDevice {public:
std::deque<uint8_t> incoming;
void feed(const std::vector<uint8_t>& bytes){incoming.insert(incoming.end(),bytes.begin(),bytes.end());}
size_t available(){return incoming.size();}
bool read_byte(uint8_t* b){if(incoming.empty())return false;*b=incoming.front();incoming.pop_front();return true;}
};}}
''')
 for ns,cl,typ,init in [('sensor','Sensor','float','0'),('text_sensor','TextSensor','std::string','{}'),('binary_sensor','BinarySensor','bool','false')]:
  put(f'esphome/components/{ns}/{ns}.h',f'''#pragma once
#include <string>
#include <vector>
namespace esphome {{ namespace {ns} {{
class {cl} {{public: {typ} state={init};std::vector<{typ}> history;
void publish_state({typ} v){{state=v;history.push_back(v);}}
}};}}}}
''')
 src=root/'components/systa_reader'
 cmd=['g++','-std=c++17','-g','-fsanitize=address,undefined','-I',str(p),'-I',str(src),str(Path(__file__).with_name('test.cpp')),*map(str,src.glob('*.cpp')),'-o',str(p/'test')]
 subprocess.run(cmd,check=True)
 import os
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
