const state = {
  mode: localStorage.getItem('ffsMode') || 'live',
  latest: null,
  selected: 'feedingmotor1',
  connected: false,
  demo: {
    t: 0,
    bagCount: 1280,
    temp: 29.2,
    hum: 56.6,
    roll: 4.8,
    weight: 247.6,
    alertUntil: 0,
    logs: [],
    alerts: [],
    series: { temp: [], hum: [], roll: [], weight: [], feed: [] },
  },
  components: {
    feedingmotor1: {
      title: 'Feeding Motor',
      description: 'Single intermittent feed motor used for stepwise film advance.',
      category: 'Machine',
      good: 'Feed pulses are occurring in controlled steps.',
      warn: 'Feed pulse timing needs attention.',
      bad: 'Feed motor fault or jam detected.',
      expected: 'Pulsed motion, never continuous.',
    },
    roll: {
      title: 'Roll Centering',
      description: 'Rack-and-pinion stepper used to recenter the roll.',
      category: 'Machine',
      good: 'Roll is centered within tolerance.',
      warn: 'PID correction is actively recentring the roll.',
      bad: 'Roll centering fault or stepper not responding.',
      expected: 'Offset should settle toward 0.0 cm.',
    },
    'vertical seal': {
      title: 'Vertical Seal Zone',
      description: 'Legacy zone kept visible for the original map layout.',
      category: 'Map zone',
      good: 'Zone is mapped and visible for inspection.',
      warn: 'Legacy zone only; not used in the simplified build.',
      bad: 'Inactive in the simplified machine.',
      expected: 'Shown for review and presentation only.',
    },
    'horizontal seal': {
      title: 'Horizontal Seal Zone',
      description: 'Legacy zone kept visible for the original map layout.',
      category: 'Map zone',
      good: 'Zone is mapped and visible for inspection.',
      warn: 'Legacy zone only; not used in the simplified build.',
      bad: 'Inactive in the simplified machine.',
      expected: 'Shown for review and presentation only.',
    },
    'filler motor': {
      title: 'Filler Motor',
      description: 'Retained as a visible map component for presentation continuity.',
      category: 'Map zone',
      good: 'Component is represented in the dashboard map.',
      warn: 'Not used in the simplified actuation set.',
      bad: 'Not installed in the simplified build.',
      expected: 'Visible, inspectable, but idle.',
    },
    'feeding motor2': {
      title: 'Feeding Motor 2',
      description: 'Legacy map region retained for the original UI structure.',
      category: 'Map zone',
      good: 'Component is represented in the dashboard map.',
      warn: 'Removed from the simplified build.',
      bad: 'Removed from the simplified build.',
      expected: 'Visible for compatibility and review.',
    },
    'dht sensor': {
      title: 'DHT Sensor',
      description: 'Ambient temperature and humidity sensor.',
      category: 'Sensor',
      good: 'Environmental readings are stable.',
      warn: 'Temperature or humidity is nearing a threshold.',
      bad: 'Sensor read failure or invalid packet.',
      expected: 'Temperature, humidity and fan trigger data.',
    },
    'thermocouple': {
      title: 'Thermocouple',
      description: 'Sealing temperature measurement channel.',
      category: 'Sensor',
      good: 'Thermocouple reading is available.',
      warn: 'Calibration offset may be required.',
      bad: 'Thermocouple not detected.',
      expected: 'Sealing temperature trend and calibration status.',
    },
    'load cell': {
      title: 'Load Cell',
      description: 'Weight measurement and bag fill monitoring.',
      category: 'Sensor',
      good: 'Weight reading is stable and calibrated.',
      warn: 'Weight is approaching the target threshold.',
      bad: 'Loadcell not calibrated.',
      expected: 'Bag fill weight and stability.',
    },
    'ultrasonic sensor': {
      title: 'Ultrasonic Sensor',
      description: 'Roll offset / position sensing used by the centering loop.',
      category: 'Sensor',
      good: 'Distance / offset is within tolerance.',
      warn: 'Offset detected and compensation is active.',
      bad: 'Distance reading out of range.',
      expected: 'Roll displacement and correction feedback.',
    },
    fan: {
      title: 'Cooling Fan',
      description: 'Triggered by environmental thresholds in the control loop.',
      category: 'Actuator',
      good: 'Fan follows automatic control rules.',
      warn: 'Fan force state is active for testing.',
      bad: 'Fan output fault or relay issue.',
      expected: 'Turns on when temperature rises above threshold.',
    },
    'esp32 link': {
      title: 'ESP32 Link',
      description: 'SPI connection between the ESP32 and Raspberry Pi.',
      category: 'System',
      good: 'ESP32 handshake and packet exchange are healthy.',
      warn: 'Legacy frame compatibility detected.',
      bad: 'SPI link failed.',
      expected: 'Packet exchange, sensor data and command ACKs.',
    },
    'spi bus': {
      title: 'SPI Bus',
      description: 'MOSI/MISO/CS communication path.',
      category: 'System',
      good: 'SPI bus is live.',
      warn: 'Frame format compatibility in use.',
      bad: 'Bus is unavailable.',
      expected: 'Consistent SPI transactions.',
    },
    'web api': {
      title: 'Web API',
      description: 'Laptop-hosted Flask dashboard used for presentation and operation.',
      category: 'System',
      good: 'Dashboard is responding.',
      warn: 'Operating in demo mode.',
      bad: 'Web route unavailable.',
      expected: 'Live controls, logs and monitoring views.',
    },
    database: {
      title: 'Data Logger',
      description: 'Stores alerts, logs and process snapshots.',
      category: 'System',
      good: 'Historical data is being collected.',
      warn: 'Using simulated log stream.',
      bad: 'Logging queue unavailable.',
      expected: 'Event history, trend records and snapshots.',
    },
  }
};

function qs(id){ return document.getElementById(id); }
function fmt(n, d=1){ const x = Number(n); return Number.isFinite(x) ? x.toFixed(d) : '0.0'; }
function clamp(v, min, max){ return Math.max(min, Math.min(max, v)); }

async function fetchJson(url, options={}) {
  try{
    const response = await fetch(url, options);
    const text = await response.text();
    try { return {ok: response.ok, status: response.status, data: JSON.parse(text), raw: text}; }
    catch { return {ok: response.ok, status: response.status, data: {ok: response.ok, text}, raw: text}; }
  }catch(err){
    return {ok:false, status:0, data:{ok:false, error:String(err)}, raw:''};
  }
}

const apiGet = (url) => fetchJson(url);
const apiPost = (url, body={}) => fetchJson(url, {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});

function badgeClass(status){
  return status === 'ok' ? 'ok' : status === 'warn' ? 'warn' : status === 'bad' ? 'bad' : 'idle';
}
function setBadge(el, status, text){
  if(!el) return;
  const c = badgeClass(status);
  el.className = `badge ${c}`;
  el.innerHTML = `<span class="status-dot ${c}"></span>${text}`;
}

function setMode(mode){
  state.mode = mode === 'demo' ? 'demo' : 'live';
  localStorage.setItem('ffsMode', state.mode);

  const pill = qs('modePill');
  if(pill){
    pill.classList.toggle('hidden', state.mode !== 'demo');
  }

  const btn = qs('ffsToggle');
  if(btn){
    btn.className = `btn pill ${state.mode === 'demo' ? 'primary' : 'ghost'}`;
    btn.title = state.mode === 'demo' ? 'Return to live mode' : 'Open FFS presentation mode';
  }

  const caption = qs('modeCaption');
  if(caption){
    caption.textContent = state.mode === 'demo' ? 'Presentation view' : 'WEB UI';
  }

  const summaryMode = qs('summaryMode');
  if(summaryMode) summaryMode.textContent = state.mode === 'demo' ? 'FFS Demo' : 'Live';
}

function toggleMode(){
  if(state.mode === 'demo'){
    state.mode = 'live';
    setMode('live');
  }else{
    state.mode = 'demo';
    setMode('demo');
    pushDemoLog('info', 'FFS presentation stream enabled');
    pushDemoAlert('warning', 'Simulated process stream active');
    stepDemo(true);
  }
  renderAll();
}

function pushDemoLog(level, message){
  const stamp = new Date().toLocaleTimeString();
  state.demo.logs.unshift({timestamp: stamp, level, tag: level.toUpperCase(), message});
  state.demo.logs = state.demo.logs.slice(0, 40);
}
function pushDemoAlert(severity, message){
  const stamp = new Date().toLocaleTimeString();
  const id = Math.floor(Math.random()*100000);
  state.demo.alerts.unshift({id, severity, category:'FFS Demo', message, resolved:false, timestamp: stamp});
  state.demo.alerts = state.demo.alerts.slice(0, 10);
}
function resolveExpiredAlerts(now){
  state.demo.alerts = state.demo.alerts.filter(a => !a.expiresAt || a.expiresAt > now);
}

function buildDemoLatest(){
  const t = state.demo.t;
  const feedOn = (t % 6) < 4;
  const fanOn = state.demo.temp >= 31.5;
  return {
    motor1: {
      current: feedOn ? 1.20 + Math.random() * 0.08 : 0.02,
      rpm: feedOn ? 58 + Math.sin(t/3) * 2 : 0,
      running: feedOn,
      state: {class: feedOn ? 'ok' : 'idle', name: feedOn ? 'Feeding pulse' : 'Paused'},
    },
    weight: {
      kg: state.demo.weight / 1000,
      grams: state.demo.weight,
      calibrated: true,
      stable: Math.abs(state.demo.weight - 250) < 1.5 && !feedOn,
    },
    temperature: {
      ambient: state.demo.temp,
      humidity: state.demo.hum,
      thermocouple: state.demo.temp + 2.4,
      tc_connected: true,
      fan_required: fanOn,
      fan_state: fanOn ? 'on' : 'auto',
    },
    roll: {
      diff_cm: state.demo.roll,
      centered: Math.abs(state.demo.roll) < 0.4,
      correction_active: Math.abs(state.demo.roll) >= 0.4,
    },
    vibration: {
      rms_g: feedOn ? 0.26 + Math.random() * 0.05 : 0.11 + Math.random() * 0.03,
      peak_g: feedOn ? 0.48 + Math.random() * 0.06 : 0.22 + Math.random() * 0.03,
      dominant_freq_hz: feedOn ? 16 + Math.random() * 3 : 5 + Math.random() * 2,
    },
    bags_counted: state.demo.bagCount,
    bag_detected: feedOn,
    bag_length_cm: Number(qs('bag_length')?.value || 18),
    env_status: fanOn ? 'Fan active' : 'Stable',
    env_sensor_ok: true,
    feed_mode: 'single_motor_stepwise',
    settings: {
      bag_length_cm: Number(qs('bag_length')?.value || 18),
      target_weight_g: Number(qs('target_weight')?.value || 250),
      seal_temp_c: Number(qs('seal_temp')?.value || 160),
      thermo_offset_c: Number(qs('thermo_offset')?.value || 0),
      fan_temp_on_c: Number(qs('fan_temp_on')?.value || 32),
      fan_temp_off_c: Number(qs('fan_temp_off')?.value || 30),
      roll_pid_kp: Number(qs('pid_kp')?.value || 0.9),
      roll_pid_ki: Number(qs('pid_ki')?.value || 0.05),
      roll_pid_kd: Number(qs('pid_kd')?.value || 0.12),
    }
  };
}

function stepDemo(force=false){
  state.demo.t += 1;
  const t = state.demo.t;
  const now = Date.now();

  // realistically changing values
  state.demo.temp = clamp(29.1 + 0.12*t + Math.sin(t/4)*0.45, 28.8, 33.4);
  state.demo.hum = clamp(57.0 - 0.08*t + Math.cos(t/5)*0.55, 50.0, 60.0);

  // roll correction with brief 3-second spikes
  if (t % 7 === 0) {
    state.demo.roll = 4.2 + Math.random() * 1.4;
    state.demo.alertUntil = now + 3000;
    pushDemoAlert('warning', 'Roll offset detected, PID correction active');
    pushDemoLog('warn', 'Roll offset detected; recentering started');
  } else if (now < state.demo.alertUntil) {
    state.demo.roll = Math.max(0.12, state.demo.roll * 0.55);
  } else {
    state.demo.roll = Math.max(0.04, state.demo.roll * 0.82);
  }

  const feedOn = (t % 6) < 4;
  if (feedOn && t % 4 === 0) pushDemoLog('info', 'Feeding pulse executed');
  if (!feedOn && t % 6 === 4) pushDemoLog('info', 'Feeding pause engaged');

  const feedDelta = feedOn ? 2.4 + Math.random() * 1.1 : -0.12;
  state.demo.weight = clamp(state.demo.weight + feedDelta, 238.0, 254.8);
  if (Math.abs(state.demo.weight - 250) < 0.8 && !feedOn) {
    state.demo.weight = 250.0 + (Math.random() * 0.4 - 0.2);
  }

  if (state.demo.temp >= 31.5 && t % 5 === 0) {
    pushDemoAlert('warning', 'Cooling fan threshold reached');
    pushDemoLog('warn', 'Fan triggered by temperature threshold');
  }
  if (t % 10 === 0) {
    state.demo.bagCount += 1;
    pushDemoLog('info', `Bag ${state.demo.bagCount} registered`);
  }

  resolveExpiredAlerts(now);
  state.latest = buildDemoLatest();

  state.demo.series.temp.push({x:t, y:state.demo.temp});
  state.demo.series.hum.push({x:t, y:state.demo.hum});
  state.demo.series.roll.push({x:t, y:state.demo.roll});
  state.demo.series.weight.push({x:t, y:state.demo.weight});
  state.demo.series.feed.push({x:t, y:feedOn ? 1 : 0});
  for (const k of Object.keys(state.demo.series)) {
    if (state.demo.series[k].length > 40) state.demo.series[k].shift();
  }

  if (force) pushDemoLog('info', 'FFS demo stream initialized');
  return state.latest;
}

function fallbackLatest(){
  return {
    motor1: {current:0,rpm:0,state:{class:'idle',name:'Idle'}, running:false},
    weight: {grams:0,kg:0,calibrated:false,stable:false},
    temperature: {ambient:0,humidity:0,thermocouple:0,tc_connected:false,fan_required:false,fan_state:'auto'},
    roll: {diff_cm:0,centered:true,correction_active:false},
    vibration: {rms_g:0,peak_g:0,dominant_freq_hz:0},
    bags_counted:0,
    bag_detected:false,
    bag_length_cm:0,
    env_status:'No Data',
    env_sensor_ok:false,
    feed_mode:'single_motor_stepwise',
    settings:{}
  };
}

async function getLiveLatest(){
  const statusResp = await apiGet('/api/status');
  const latestResp = await apiGet('/api/sensors/latest');

  const status = statusResp.data || {};
  const latest = latestResp.data?.data || status.latest || latestResp.data || null;
  state.connected = !!status.ok;

  const top = qs('topStatus');
  setBadge(top, state.connected ? 'ok' : 'warn', state.connected ? 'Connected' : 'Connecting');

  const sys = qs('sysStatus'); if(sys) sys.textContent = state.connected ? 'Online' : 'Degraded';
  const sens = qs('sensorStatus'); if(sens) sens.textContent = latest ? 'Live' : 'Waiting';
  const feed = qs('feedStatus'); if(feed) feed.textContent = latest?.feed_mode || 'Single-Step';
  const roll = qs('rollStatus'); if(roll) roll.textContent = latest?.roll?.centered ? 'Centered' : 'Offset';

  if (latest) state.latest = latest;

  const alertsResp = await apiGet('/api/alerts?limit=25&unresolved=true');
  const logsResp = await apiGet('/api/logs/history?hours=24&limit=30');
  const alerts = Array.isArray(alertsResp.data) ? alertsResp.data : (alertsResp.data?.data || []);
  const logs = Array.isArray(logsResp.data) ? logsResp.data : (logsResp.data?.data || []);

  renderSharedPanels(state.latest || fallbackLatest(), alerts, logs);
}

function updateTopStatus(latest){
  const top = qs('topStatus');
  if(!top) return;
  if(state.mode === 'demo'){
    setBadge(top, 'ok', 'Connected');
    return;
  }
  const warning = latest && ((latest.roll && !latest.roll.centered) || (latest.temperature && latest.temperature.fan_required));
  setBadge(top, warning ? 'warn' : 'ok', warning ? 'Monitoring' : 'Connected');
}

function buildSnapshot(latest){
  const temp = Number(latest.temperature?.ambient || 0);
  const hum = Number(latest.temperature?.humidity || 0);
  const tc = Number(latest.temperature?.thermocouple || 0);
  const weight = Number(latest.weight?.grams || 0);
  const roll = Number(latest.roll?.diff_cm || 0);
  const feedRunning = !!latest.motor1?.running;
  const bags = Number(latest.bags_counted || 0);
  const fan = latest.temperature?.fan_state || (latest.temperature?.fan_required ? 'on' : 'auto');
  const bagsPerHour = state.mode === 'demo'
    ? clamp(150 + (feedRunning ? 25 : 0) + Math.sin(state.demo.t/6) * 4, 120, 180)
    : clamp(feedRunning ? 138 : 126, 90, 180);

  return {
    items: [
      ['Feed mode', latest.feed_mode || 'single_motor_stepwise'],
      ['Temperature', `${fmt(temp,1)} °C`],
      ['Humidity', `${fmt(hum,1)} %`],
      ['Thermocouple', `${fmt(tc,1)} °C`],
      ['Weight', `${fmt(weight,1)} g`],
      ['Roll offset', `${fmt(roll,2)} cm`],
      ['Fan state', fan],
      ['Bags counted', `${bags}`],
      ['Estimated output', `${fmt(bagsPerHour,0)} bags/hour`],
      ['Feed motor', feedRunning ? 'Running in pulse' : 'Paused'],
    ],
    ranges: [
      ['Ambient temperature', '29.0 – 32.5 °C'],
      ['Humidity', '50 – 60 %'],
      ['Target fill', '250 g'],
      ['Roll tolerance', '±0.40 cm'],
      ['Fan threshold', '31.5 °C'],
      ['Recovery time', 'About 3 seconds'],
    ]
  };
}

function renderSharedPanels(latest, alerts, logs){
  if(!latest) latest = fallbackLatest();

  const summary = qs('summaryBox');
  if(summary){
    summary.innerHTML = `
      <div class="kv"><span>Feed motor</span><strong>${latest.motor1?.state?.name || 'Unknown'} (${fmt(latest.motor1?.rpm || 0, 1)} RPM)</strong></div>
      <div class="kv"><span>Weight</span><strong>${fmt(latest.weight?.grams ?? (latest.weight?.kg||0)*1000, 1)} g</strong></div>
      <div class="kv"><span>Temperature</span><strong>${fmt(latest.temperature?.ambient || 0, 1)} °C</strong></div>
      <div class="kv"><span>Humidity</span><strong>${fmt(latest.temperature?.humidity || 0, 1)} %</strong></div>
      <div class="kv"><span>Roll offset</span><strong>${fmt(latest.roll?.diff_cm || 0, 2)} cm</strong></div>
      <div class="kv"><span>Bags counted</span><strong>${latest.bags_counted ?? 0}</strong></div>
      <div class="kv"><span>Mode</span><strong>${state.mode === 'demo' ? 'FFS demo' : 'Live'}</strong></div>
    `;
  }

  const snap = buildSnapshot(latest);
  const snapBox = qs('presentationBox');
  if(snapBox){
    snapBox.innerHTML = `<div class="snapshot-grid">${
      snap.items.map(([k,v]) => `<div class="snapshot-item"><div class="k">${k}</div><div class="v">${v}</div></div>`).join('')
    }</div>`;
  }
  const rangesBox = qs('rangesBox');
  if(rangesBox){
    rangesBox.innerHTML = `<div class="snapshot-grid">${
      snap.ranges.map(([k,v]) => `<div class="snapshot-item"><div class="k">${k}</div><div class="v">${v}</div></div>`).join('')
    }</div><div class="legend-pills"><span>Reusable in slides</span><span>Realistic operating window</span><span>Visible in demo mode</span></div>`;
  }

  const weightStat = qs('weightStat'); if(weightStat) weightStat.textContent = fmt(latest.weight?.grams || 0, 1);
  const ambTemp = qs('ambTemp'); if(ambTemp) ambTemp.textContent = fmt(latest.temperature?.ambient || 0, 1);
  const hum = qs('hum'); if(hum) hum.textContent = fmt(latest.temperature?.humidity || 0, 1);
  const tcTemp = qs('tcTemp'); if(tcTemp) tcTemp.textContent = fmt(latest.temperature?.thermocouple || 0, 1);

  const weightBox = qs('weightBox');
  if(weightBox){
    weightBox.innerHTML = `
      <div class="kv"><span>Current mass</span><strong>${fmt(latest.weight?.grams || 0, 1)} g</strong></div>
      <div class="kv"><span>Stable</span><strong>${latest.weight?.stable ? 'Yes' : 'No'}</strong></div>
      <div class="kv"><span>Calibrated</span><strong>${latest.weight?.calibrated ? 'Yes' : 'No'}</strong></div>
      <div class="kv"><span>Target</span><strong>${fmt(latest.settings?.target_weight_g || 250, 1)} g</strong></div>
    `;
  }
  const rollBox = qs('rollBox');
  if(rollBox){
    rollBox.innerHTML = `
      <div class="kv"><span>Offset</span><strong>${fmt(latest.roll?.diff_cm || 0, 2)} cm</strong></div>
      <div class="kv"><span>Centered</span><strong>${latest.roll?.centered ? 'Yes' : 'No'}</strong></div>
      <div class="kv"><span>Action</span><strong>${latest.roll?.correction_active ? 'Stepper correction active' : 'None'}</strong></div>
      <div class="kv"><span>Status</span><strong>${latest.roll?.centered ? 'Within tolerance' : 'Adjusting'}</strong></div>
    `;
  }

  renderAlertsLogs(alerts, logs);
  renderComponentList();
  selectComponent(state.selected);
  updateTopStatus(latest);
  layoutOverlays();
  renderCharts();
}

function renderAlertsLogs(alerts, logs){
  const aBox = qs('alertsBox');
  const lBox = qs('logsBox');

  if(aBox){
    const list = state.mode === 'demo' ? state.demo.alerts : alerts;
    aBox.innerHTML = list.length
      ? list.map(a => `<div class="log-item"><div><strong>${a.category || 'Alert'}</strong> <span class="subtle">${a.severity || 'info'}</span></div><div>${a.message}</div></div>`).join('')
      : '<div class="subtle">No active alerts.</div>';
  }

  if(lBox){
    const list = state.mode === 'demo' ? state.demo.logs : logs;
    lBox.innerHTML = list.length
      ? list.map(l => `<div class="log-item"><div><strong>${l.tag || 'LOG'}</strong> <span class="subtle">${l.level || 'info'}</span></div><div>${l.message}</div></div>`).join('')
      : '<div class="subtle">No logs yet.</div>';
  }
}

function componentStatus(name, latest){
  const comp = state.components[name] || {title:name, description:'No description available.', good:'Healthy.', warn:'Attention needed.', bad:'Unavailable.', expected:'Inspectable.'};

  if(name === 'feedingmotor1'){
    const motor = latest.motor1 || {};
    const running = !!motor.running;
    const cls = motor.state?.class || (running ? 'ok' : 'idle');
    return {
      status: cls === 'bad' ? 'bad' : cls === 'warn' ? 'warn' : running ? 'ok' : 'idle',
      label: running ? 'Running' : 'Idle',
      note: running ? comp.good : 'Feed motor stands by between pulses.',
      meta: `RPM ${fmt(motor.rpm || 0,1)} | Current ${fmt(motor.current || 0,2)} A`
    };
  }

  if(name === 'roll'){
    const diff = Number(latest.roll?.diff_cm || 0);
    return {
      status: latest.roll?.centered ? 'ok' : (Math.abs(diff) < 1.0 ? 'warn' : 'bad'),
      label: latest.roll?.centered ? 'Centered' : 'Adjusting',
      note: latest.roll?.centered ? comp.good : comp.warn,
      meta: `Offset ${fmt(diff,2)} cm | PID ${latest.roll?.correction_active ? 'active' : 'idle'}`
    };
  }

  if(name === 'fan'){
    const fanOn = latest.temperature?.fan_required || latest.temperature?.fan_state === 'on';
    return {
      status: fanOn ? 'ok' : 'idle',
      label: fanOn ? 'On / Auto' : 'Auto',
      note: fanOn ? 'Fan responds to temperature threshold.' : 'Fan remains ready for threshold control.',
      meta: `Temperature ${fmt(latest.temperature?.ambient || 0,1)} °C`
    };
  }

  if(name === 'dht sensor' || name === 'thermocouple' || name === 'load cell' || name === 'ultrasonic sensor') {
    return {
      status: 'ok',
      label: 'Available',
      note: comp.good,
      meta: comp.expected
    };
  }

  if(name === 'esp32 link' || name === 'spi bus' || name === 'web api' || name === 'database') {
    return {
      status: state.mode === 'demo' && (name === 'web api' || name === 'database') ? 'warn' : 'ok',
      label: state.mode === 'demo' && (name === 'web api' || name === 'database') ? 'Demo' : 'Online',
      note: state.mode === 'demo' && (name === 'web api' || name === 'database') ? 'Demo data stream active.' : comp.good,
      meta: comp.expected
    };
  }

  // legacy map zones remain inspectable
  return {
    status: 'idle',
    label: 'Legacy',
    note: comp.warn,
    meta: comp.expected
  };
}

function renderComponentList(){
  const box = qs('componentList');
  if(!box) return;
  const latest = state.latest || fallbackLatest();
  box.innerHTML = '';
  Object.keys(state.components).forEach(name => {
    const info = componentStatus(name, latest);
    const item = document.createElement('div');
    item.className = 'component-pill' + (state.selected === name ? ' active' : '');
    item.innerHTML = `
      <div>
        <div style="font-weight:700">${state.components[name].title}</div>
        <div class="subtle small">${state.components[name].description}</div>
      </div>
      <span class="badge ${badgeClass(info.status)}">${info.label}</span>
    `;
    item.onclick = () => selectComponent(name);
    box.appendChild(item);
  });
}

function selectComponent(name){
  state.selected = name;
  const latest = state.latest || fallbackLatest();
  const comp = state.components[name] || {title:name, description:'No description.'};
  const info = componentStatus(name, latest);

  const title = qs('selectedTitle'); if(title) title.textContent = comp.title;
  const desc = qs('selectedDesc'); if(desc) desc.textContent = comp.description;
  const badge = qs('selectedBadge'); if(badge) setBadge(badge, info.status, info.label);
  const meta = qs('selectedMeta');
  if(meta){
    meta.innerHTML = `
      <div class="kv"><span>Category</span><strong>${comp.category || 'Component'}</strong></div>
      <div class="kv"><span>Expected</span><strong>${comp.expected || 'Inspectable.'}</strong></div>
      <div class="kv"><span>Live note</span><strong>${info.note}</strong></div>
      <div class="kv"><span>Detail</span><strong>${info.meta}</strong></div>
    `;
  }
  renderComponentList();
  updateHotspotStyles();
}

function overlayBox(id, rect, status){
  const el = qs(id);
  if(!el) return;
  el.style.left = rect.x + 'px';
  el.style.top = rect.y + 'px';
  el.style.width = rect.w + 'px';
  el.style.height = rect.h + 'px';
  el.className = `hotspot ${status}`;
}

function layoutOverlays(){
  const img = qs('machineImg');
  if(!img) return;
  const W = img.clientWidth;
  const H = img.clientHeight;
  const sx = W / 778.0;
  const sy = H / 865.0;

  const rects = {
    'horizontal seal': {x:243*sx, y:551*sy, w:(346-243)*sx, h:(607-551)*sy},
    'vertical seal': {x:280*sx, y:422*sy, w:(309-280)*sx, h:(528-422)*sy},
    'feedingmotor1': {x:253*sx, y:397*sy, w:(282-253)*sx, h:(497-397)*sy},
    'feeding motor2': {x:324*sx, y:421*sy, w:(369-324)*sx, h:(521-421)*sy},
    'filler motor': {x:282*sx, y:159*sy, w:(380-282)*sx, h:(257-159)*sy},
    'roll': {x:429*sx, y:482*sy, w:(538-429)*sx, h:(550-482)*sy},
  };

  const latest = state.latest || fallbackLatest();
  overlayBox('overlay-feedingmotor1', rects['feedingmotor1'], latest.motor1?.state?.class === 'bad' ? 'bad' : latest.motor1?.running ? 'ok' : 'idle');
  overlayBox('overlay-vertical-seal', rects['vertical seal'], 'idle');
  overlayBox('overlay-horizontal-seal', rects['horizontal seal'], 'idle');
  overlayBox('overlay-roll', rects['roll'], latest.roll?.centered ? 'ok' : (Math.abs(Number(latest.roll?.diff_cm || 0)) < 1.0 ? 'warn' : 'bad'));
  overlayBox('overlay-filler-motor', rects['filler motor'], 'idle');
  overlayBox('overlay-feedingmotor2', rects['feeding motor2'], 'idle');

  const mapping = [
    ['overlay-feedingmotor1', 'feedingmotor1'],
    ['overlay-vertical-seal', 'vertical seal'],
    ['overlay-horizontal-seal', 'horizontal seal'],
    ['overlay-filler-motor', 'filler motor'],
    ['overlay-feedingmotor2', 'feeding motor2'],
    ['overlay-roll', 'roll'],
  ];
  mapping.forEach(([id, part]) => {
    const el = qs(id);
    if(!el) return;
    el.onclick = () => selectComponent(part);
    el.title = part;
  });
}

function updateHotspotStyles(){ layoutOverlays(); }

function drawChart(canvas, series, opts={}){
  if(!canvas) return;
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const cssW = canvas.clientWidth || canvas.width || 600;
  const cssH = canvas.clientHeight || canvas.height || 220;
  if(canvas.width !== cssW * dpr || canvas.height !== cssH * dpr){
    canvas.width = cssW * dpr;
    canvas.height = cssH * dpr;
  }
  ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,cssW,cssH);

  const pad = 20;
  const innerW = cssW - pad*2;
  const innerH = cssH - pad*2;
  const all = series.flatMap(s => s.values.map(v => v.y));
  const min = opts.min ?? (all.length ? Math.min(...all) : 0);
  const max = opts.max ?? (all.length ? Math.max(...all) : 1);
  const span = Math.max(1e-6, max - min);

  ctx.strokeStyle = '#e5ecf6';
  ctx.lineWidth = 1;
  for(let i=0; i<=4; i++){
    const y = pad + innerH * (i/4);
    ctx.beginPath(); ctx.moveTo(pad, y); ctx.lineTo(cssW-pad, y); ctx.stroke();
  }

  ctx.fillStyle = '#64748b';
  ctx.font = '12px Segoe UI, Arial';
  ctx.fillText(`${max.toFixed(opts.digits ?? 1)}`, 4, pad + 4);
  ctx.fillText(`${min.toFixed(opts.digits ?? 1)}`, 4, cssH - 6);

  const n = Math.max(2, ...series.map(s => s.values.length));
  series.forEach(s => {
    if(!s.values.length) return;
    ctx.strokeStyle = s.color || '#2563eb';
    ctx.lineWidth = 2.5;
    ctx.beginPath();
    s.values.forEach((p, idx) => {
      const x = pad + innerW * (idx / Math.max(1, n - 1));
      const y = pad + innerH - ((p.y - min) / span) * innerH;
      if(idx===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
    });
    ctx.stroke();
    s.values.forEach((p, idx) => {
      const x = pad + innerW * (idx / Math.max(1, n - 1));
      const y = pad + innerH - ((p.y - min) / span) * innerH;
      ctx.fillStyle = s.color || '#2563eb';
      ctx.beginPath(); ctx.arc(x, y, 2.5, 0, Math.PI*2); ctx.fill();
    });
  });
}

function renderCharts(){
  const tempCanvas = qs('chartTemp');
  const rollCanvas = qs('chartRoll');
  const weightCanvas = qs('chartWeight');
  if(!tempCanvas && !rollCanvas && !weightCanvas) return;

  const tSeries = state.demo.series.temp.slice(-30);
  const hSeries = state.demo.series.hum.slice(-30);
  const rSeries = state.demo.series.roll.slice(-30);
  const wSeries = state.demo.series.weight.slice(-30);
  const fSeries = state.demo.series.feed.slice(-30);

  drawChart(tempCanvas, [
    {values:tSeries, color:'#2563eb'},
    {values:hSeries, color:'#16a34a'},
  ], {min:20, max:65, digits:0});

  drawChart(rollCanvas, [
    {values:rSeries, color:'#d97706'}
  ], {min:0, max:6, digits:1});

  drawChart(weightCanvas, [
    {values:wSeries, color:'#1d4ed8'},
    {values:fSeries.map(p => ({x:p.x, y: 238 + p.y*16})), color:'#dc2626'},
  ], {min:230, max:260, digits:0});
}

async function refreshSettings(){
  const r = await apiGet('/api/control/settings');
  const s = r.data?.settings || r.data || {};
  if(!s) return;
  const mapping = {
    bag_length_cm:'bag_length', target_weight_g:'target_weight', seal_temp_c:'seal_temp', thermo_offset_c:'thermo_offset',
    fan_temp_on_c:'fan_temp_on', fan_temp_off_c:'fan_temp_off',
    roll_pid_kp:'pid_kp', roll_pid_ki:'pid_ki', roll_pid_kd:'pid_kd',
    hold_time_s:'hold_time',
  };
  for(const [k,id] of Object.entries(mapping)){
    const el = qs(id);
    if(el && s[k] !== undefined) el.value = s[k];
  }
}

async function saveSettings(){
  const payload = {
    bag_length_cm: Number(qs('bag_length').value || 18),
    target_weight_g: Number(qs('target_weight').value || 250),
    seal_temp_c: Number(qs('seal_temp').value || 160),
    thermo_offset_c: Number(qs('thermo_offset').value || 0),
    fan_temp_on_c: Number(qs('fan_temp_on').value || 32),
    fan_temp_off_c: Number(qs('fan_temp_off').value || 30),
    roll_pid_kp: Number(qs('pid_kp').value || 0.9),
    roll_pid_ki: Number(qs('pid_ki').value || 0.05),
    roll_pid_kd: Number(qs('pid_kd').value || 0.12),
    hold_time_s: Number(qs('hold_time').value || 1.5),
  };

  if(state.mode === 'demo'){
    state.latest = state.latest || fallbackLatest();
    state.latest.settings = payload;
    pushDemoLog('info', 'Settings updated in presentation mode');
    renderSharedPanels(state.latest, state.demo.alerts, state.demo.logs);
    return;
  }
  
  return sendControl('/api/control/settings', payload);
}

function pushToast(message){
  const el = document.createElement('div');
  el.className = 'callout';
  el.style.position = 'fixed';
  el.style.right = '18px';
  el.style.bottom = '18px';
  el.style.zIndex = '9999';
  el.style.boxShadow = '0 12px 30px rgba(15,23,42,.16)';
  el.innerHTML = `<strong>${message}</strong>`;
  document.body.appendChild(el);
  setTimeout(() => el.remove(), 1600);
}

async function sendControl(url, body={}){
  if(state.mode === 'demo'){
    pushDemoLog('info', `Demo control: ${url.split('/').pop().replace(/_/g, ' ')}`);
    if(url.includes('emergency_stop')) {
      state.demo.roll = 0.12;
      state.demo.weight = Math.max(238, state.demo.weight - 0.4);
      pushDemoAlert('warning', 'Emergency stop requested in presentation mode');
    }
    renderSharedPanels(state.latest || fallbackLatest(), state.demo.alerts, state.demo.logs);
    return {ok:true, demo:true};
  }
  const r = await apiPost(url, body);
  if(r.ok) pushToast('Control sent to Pi');
  else pushToast('Control request returned an error');
  return r;
}

async function postAction(url){ return sendControl(url, {}); }
async function startLoadcellCalibration(){ return sendControl('/api/calibrate/loadcell/start', {known_weight_g: Number(qs('knownWeight').value)}); }
async function confirmLoadcellCalibration(){ return sendControl('/api/calibrate/loadcell/confirm', {measured_weight_g: Number(qs('measuredWeight').value)}); }
async function cancelLoadcellCalibration(){ return sendControl('/api/calibrate/loadcell/cancel', {}); }

async function checkThermocouple(){
  const status = qs('calStatus');
  const currentInput = qs('currentTemp');
  const refInput = qs('refTemp');

  if(state.mode === 'demo'){
    if(status) status.value = 'OK (demo)';
    pushDemoLog('info', 'Thermocouple check passed in presentation mode');
    return;
  }
  const r = await apiGet('/api/calibrate/thermocouple/check');
  const payload = r.data || {};

  if(r.ok) {
    if(currentInput && payload.current_temp_c !== undefined) {
      currentInput.value = payload.current_temp_c;
    }
    if(refInput && payload.reference_temp_c !== undefined) {
      refInput.value = payload.reference_temp_c;
    }
  }

  if(status){
    status.value = payload.status || payload.message || (r.ok ? 'OK' : 'ERROR');
  }
}
async function calibrateThermocouple(){
  const payload = {
    reference_temp_c: Number(qs('refTemp').value),
    current_temp_c: Number(qs('currentTemp').value),
    offset_c: Number(qs('calOffset').value),
  };
  const status = qs('calStatus');
  if(state.mode === 'demo'){
    if(status) status.value = `Saved offset ${fmt(payload.offset_c,1)} °C (demo)`;
    pushDemoLog('info', 'Thermocouple offset saved in presentation mode');
    return;
  }
  const r = await apiPost('/api/calibrate/thermocouple', payload);
  const text = r.data?.message || r.data?.status || r.data?.text || (r.ok ? 'Saved' : 'Error');
  if(status) status.value = text;
}

async function refreshLogs(){
  if(state.mode === 'demo'){
    renderSharedPanels(state.latest || fallbackLatest(), state.demo.alerts, state.demo.logs);
    return;
  }
  const hours = qs('logHours') ? qs('logHours').value : 24;
  const logsResp = await apiGet(`/api/logs/history?hours=${encodeURIComponent(hours)}&limit=100`);
  const alertsResp = await apiGet('/api/alerts?limit=25&unresolved=true');
  const logs = Array.isArray(logsResp.data) ? logsResp.data : (logsResp.data?.data || []);
  const alerts = Array.isArray(alertsResp.data) ? alertsResp.data : (alertsResp.data?.data || []);
  renderSharedPanels(state.latest || fallbackLatest(), alerts, logs);
}

function renderAll(){
  const latest = state.mode === 'demo' ? (state.latest || buildDemoLatest()) : (state.latest || fallbackLatest());
  const top = qs('topStatus');
  if(top) setBadge(top, state.mode === 'demo' ? 'ok' : 'ok', state.mode === 'demo' ? 'Connected' : (state.connected ? 'Connected' : 'Connecting'));
  renderSharedPanels(latest, state.demo.alerts, state.demo.logs);
}

async function refreshLive(){
  if(state.mode === 'demo'){
    stepDemo();
    renderSharedPanels(state.latest || fallbackLatest(), state.demo.alerts, state.demo.logs);
    return;
  }
  await getLiveLatest();
}

function overlayResize(){
  layoutOverlays();
  renderCharts();
}

async function boot(){
  setMode(state.mode);

  const btn = qs('ffsToggle');
  if(btn) btn.addEventListener('click', toggleMode);

  renderComponentList();
  selectComponent(state.selected);

  window.addEventListener('resize', overlayResize);
  const machineImg = qs('machineImg');
  if(machineImg) machineImg.addEventListener('load', overlayResize);

  try { await refreshSettings(); } catch {}
  if(state.mode === 'demo'){
    stepDemo(true);
    renderSharedPanels(state.latest || fallbackLatest(), state.demo.alerts, state.demo.logs);
  } else {
    await refreshLive();
  }

  setInterval(() => {
    if(state.mode === 'demo'){
      stepDemo();
      renderSharedPanels(state.latest || fallbackLatest(), state.demo.alerts, state.demo.logs);
    } else {
      refreshLive();
    }
  }, 3000);
}

window.qs = qs;
window.postAction = postAction;
window.saveSettings = saveSettings;
window.refreshSettings = refreshSettings;
window.sendControl = sendControl;
window.startLoadcellCalibration = startLoadcellCalibration;
window.confirmLoadcellCalibration = confirmLoadcellCalibration;
window.cancelLoadcellCalibration = cancelLoadcellCalibration;
window.checkThermocouple = checkThermocouple;
window.calibrateThermocouple = calibrateThermocouple;
window.refreshLogs = refreshLogs;

document.addEventListener('DOMContentLoaded', boot);
