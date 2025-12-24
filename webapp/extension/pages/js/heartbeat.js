(function(){
  // Use the heartbeat endpoint implemented on the device
  const DEVICE_URL = 'http://192.168.4.1/heartbeat';
  const INTERVAL_MS = 5000;
  // start pessimistic: assume offline until heartbeat proves otherwise
  let online = false;
  let fileTabApplied = false;

  // Global device info for webapp. Defaults as requested.
  window.deviceInfo = window.deviceInfo || { hw: 'M5Stack PaperS3', firmware: 'ReadPaper', version: 'V1.3' };

  function log(){ if(window.console) console.debug.apply(console, ['[heartbeat]'].concat(Array.from(arguments))); }

  async function checkOnce(){
    const controller = new AbortController();
    const timer = setTimeout(()=>controller.abort(), 2500);
    try{
      // call the device heartbeat endpoint with CORS; device returns JSON and proper CORS headers
      const r = await fetch(DEVICE_URL, {mode:'cors', cache:'no-store', signal: controller.signal});
      if (r && r.ok) {
        // Only treat device as online when the heartbeat returns a JSON
        // payload containing an explicit status === 'OK' (case-insensitive).
        // Examples that should be considered OFFLINE:
        // - HTTP 204 (No Content)
        // - HTTP 200 with JSON missing `status` or with status != 'OK'
        try {
          const j = await r.json();
          const statusVal = j && j.status ? String(j.status).toLowerCase() : null;
          if (statusVal === 'ok') {
            // Accept and update deviceInfo only when status === 'OK'
            if (j.hw) window.deviceInfo.hw = j.hw;
            if (j.firmware) window.deviceInfo.firmware = j.firmware;
            if (j.version) window.deviceInfo.version = j.version;
            const devEl = document.getElementById('device-info');
            if (devEl) devEl.textContent = (window.deviceInfo && window.deviceInfo.firmware ? window.deviceInfo.firmware : 'ReadPaper') + ' ' +
                                         (window.deviceInfo && window.deviceInfo.version ? window.deviceInfo.version : 'V1.3') + ' @ ' +
                                         (window.deviceInfo && window.deviceInfo.hw ? window.deviceInfo.hw : 'M5Stack PaperS3');
            setOnline(true);
          } else {
            // JSON present but not reporting OK — keep offline
            log('heartbeat: JSON status not OK, treating as offline', j);
            setOnline(false);
          }
        } catch (e) {
          // parse failure (e.g. 204 No Content) — treat as offline
          log('heartbeat: failed to parse JSON, treating as offline', e && e.message ? e.message : e);
          setOnline(false);
        }
      } else {
        setOnline(false);
      }
    }catch(e){
      setOnline(false);
    }finally{ clearTimeout(timer); }
  }

  function applyStateToFileTab(state){
    const fileTab = document.querySelector('.tabs a[href="filemanager.html"]');
    // Because header may initially point filemanager->welcome, try either href
    let fileTabEl = fileTab || document.querySelector('.tabs a.disabled-until-heartbeat') || document.querySelector('.tabs a[href="welcome.html"]');
    if(!fileTabEl) return false;
    const statusEl = document.getElementById('connection-status');
    if(!state){
      fileTabEl.classList.remove('text-dark');
      fileTabEl.classList.add('text-light');
      fileTabEl.classList.add('disabled-until-heartbeat');
      fileTabEl.setAttribute('href', 'welcome.html');
      fileTabEl.setAttribute('aria-disabled', 'true');
      if(statusEl) statusEl.textContent = '设备不在线，请确认设备已处于热点模式，而且本机已经连接。';
      log('device offline - UI applied');
    } else {
      fileTabEl.classList.remove('text-light');
      fileTabEl.classList.remove('disabled-until-heartbeat');
      fileTabEl.classList.add('text-dark');
      fileTabEl.setAttribute('href', 'filemanager.html');
      fileTabEl.setAttribute('aria-disabled', 'false');
      if(statusEl) statusEl.textContent = '';
      log('device online - UI applied');
    }
    return true;
  }

  function setOnline(state){
    if(online === state) return;
    online = state;
    // show/hide device info element depending on online state
    try{
      const devEl = document.getElementById('device-info');
      if(devEl){
        if(state){
          // populate from window.deviceInfo if available
          try{
            devEl.textContent = (window.deviceInfo && window.deviceInfo.firmware ? window.deviceInfo.firmware : 'ReadPaper') + ' ' +
                                (window.deviceInfo && window.deviceInfo.version ? window.deviceInfo.version : 'V1.3') + ' @ ' +
                                (window.deviceInfo && window.deviceInfo.hw ? window.deviceInfo.hw : 'M5Stack PaperS3');
          }catch(_){ /* ignore */ }
          devEl.classList.add('show');
        } else {
          devEl.classList.remove('show');
        }
      }
    }catch(_){ }

    // try to apply immediately; if header not yet loaded, ensureApply will pick it up
    if(applyStateToFileTab(state)) fileTabApplied = true;
  }

  function offlineClickHandler(e){
    // redirect to welcome and show message
    e.preventDefault();
    try{ window.location.href = 'welcome.html'; }catch(_){ /* ignore */ }
    const statusEl = document.getElementById('connection-status');
    if(statusEl) statusEl.textContent = '设备不在线：已跳转到首页，请确认 Wi‑Fi 连接并重试。';
  }

  // delegated click guard: capture clicks on filemanager links even before header exists
  document.addEventListener('click', function(e){
    try{
      const a = e.target.closest && e.target.closest('.tabs a[href="filemanager.html"]');
      if(!a) return;
      if(!online){
        e.preventDefault();
        // behave like offline click handler
        try{ window.location.href = 'welcome.html'; }catch(_){ }
        const statusEl = document.getElementById('connection-status');
        if(statusEl) statusEl.textContent = '设备不在线：已跳转到首页，请确认 Wi‑Fi 连接并重试。';
      }
    }catch(err){ /* ignore */ }
  }, true);

  // Start heartbeat: run immediately if DOM already ready, otherwise wait for DOMContentLoaded.
  function startHeartbeat(){
    // initial check — do one immediately, then periodic checks
    checkOnce();
    setInterval(checkOnce, INTERVAL_MS);

    // ensure that the fileTab's visual state is applied as soon as header partial is inserted
    const ensureInterval = setInterval(()=>{
      if(applyStateToFileTab(online)){
        fileTabApplied = true;
        clearInterval(ensureInterval);
      }
    }, 250);
    // stop trying after 10s to avoid infinite polling
    setTimeout(()=>{ if(!fileTabApplied) clearInterval(ensureInterval); }, 10000);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', startHeartbeat);
  } else {
    // DOM already ready (likely because script was injected after DOMContentLoaded)
    startHeartbeat();
  }

})();
