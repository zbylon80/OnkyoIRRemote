#pragma once
void alarmSchedulerChecks() {
  AlarmScheduler scheduler;
  assert(!scheduler.settings.slots[0].enabled && !scheduler.settings.slots[1].enabled);
  scheduler.storageReady = true;
  for (auto &slot : scheduler.settings.slots) { slot.enabled = 1; slot.dueDay = 20261002; }
  scheduler.settings.slots[1].source = 1;
  AlarmSettings stored;
  std::vector<std::string> sent;
  bool writable = true, transmit = true;
  auto persist = [&](const AlarmSettings &value) { if (!writable) return false; stored = value; sent.push_back("save"); return true; };
  auto send = [&](const char *command) { assert(!stored.slots[0].enabled || !stored.slots[1].enabled); sent.push_back(command); return transmit; };
  auto tick = [&](uint64_t ms, tm when, bool clock=true, bool blocked=false) { scheduler.tick(ms,&when,clock,blocked,persist,send); };
  auto arm = [&](int id, int day) { scheduler.settings.slots[id].enabled=1; scheduler.settings.slots[id].dueDay=20261000+day; };
  tick(0,local(2),false); tick(0,local(1,59)); assert(sent.empty());
  tick(0,local(2)); assert((sent==std::vector<std::string>{"save","POWER"})); assert(!stored.slots[0].enabled);
  tick(1000,local(2)); assert(sent.size()==2);
  scheduler.settings=stored; tick(2000,local(2)); assert(sent.size()==2);
  tick(3000,local(7)); assert(scheduler.pending() && sent.back()=="POWER" && !stored.slots[1].enabled);
  tick(4999,local(7)); assert(sent.size()==4);
  tick(5000,local(7)); assert(!scheduler.pending() && sent.back()=="CD");
  tick(6000,local(7)); tick(7000,local(2)); tick(8000,local(7,0,0,3)); assert(sent.size()==5); // Never daily.
  arm(1,3); tick(9000,local(7,0,0,2)); assert(sent.size()==5); // Future day, clock rollback.
  tick(10000,local(7,0,0,3)); scheduler.cancelSource(); tick(12000,local(7,0,0,3)); assert(sent.back()=="POWER");
  arm(1,4); tick(13000,local(7,0,0,4)); tick(17000,local(7,0,0,4)); assert(!scheduler.pending() && sent.back()=="POWER");
  arm(1,5); tick(18000,local(7,1,0,5)); assert(!scheduler.settings.slots[1].enabled && sent.back()=="save"); // Missed minute.
  arm(1,6); tick(19000,local(7,0,0,6),true,true); assert(!scheduler.pending() && !scheduler.settings.slots[1].enabled);
  auto count=sent.size(); tick(20000,local(7,0,0,6)); assert(sent.size()==count);
  arm(0,7); writable=false; tick(21000,local(2,0,0,7)); assert(!scheduler.storageReady && sent.size()==count);
  writable=true; tick(22000,local(2,0,0,7)); assert(sent.size()==count);
  scheduler.storageReady=true; scheduler.settings.slots[0].enabled=0; arm(1,8); transmit=false;
  tick(23000,local(7,0,0,8)); assert(!scheduler.pending() && !scheduler.settings.slots[1].enabled);
  count=sent.size(); tick(24000,local(7,0,0,8)); assert(sent.size()==count);
  arm(1,9); tm near=local(6,58,0,9),far=local(6,57,0,9),wrongDay=local(6,58,0,8);
  assert(scheduler.blocksRestart(&near) && !scheduler.blocksRestart(&far) && !scheduler.blocksRestart(&wrongDay));
  assert(nextAlarmDay(&near,7,0)==20261009);
  assert(nextAlarmDay(&near,6,58)==20261010); // Current minute is already started.
  tm end=local(23,59,0,31,12); assert(nextAlarmDay(&end,2,0)==20270101);
  tm leap=local(23,59,0,28,2); leap.tm_year=124; assert(nextAlarmDay(&leap,2,0)==20240229);
  AlarmSettings legacy; legacy.version=1; legacy.slots[1].enabled=1; legacy.slots[1].dueDay=20261002;
  assert(upgradeAlarmSettings(legacy) && legacy.version==2 && legacy.slots[1].enabled && legacy.slots[1].dueDay==0);
  legacy.version=99; assert(!upgradeAlarmSettings(legacy));
  auto bad=scheduler.settings; bad.slots[1].hour=24; assert(!validAlarms(bad));
  bad=scheduler.settings; bad.slots[1].source=ALARM_SOURCE_COUNT; assert(!validAlarms(bad));
  std::cout << "One-shot alarms: date rollover, once-only execution, persistence, source timing, migration and failures passed.\n";
}
