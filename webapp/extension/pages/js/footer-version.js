(function(){
  try{
    const el = document.getElementById('curversion');
    if(!el) { console.debug('[footer] curversion element not found'); return; }

    // If chrome.runtime is available (extension context), use it.
    if (typeof chrome !== 'undefined' && chrome.runtime && typeof chrome.runtime.getManifest === 'function') {
      try{
        const m = chrome.runtime.getManifest();
        if (m && m.version) { el.textContent = 'v' + m.version + ' '; console.debug('[footer] got version from chrome.runtime.getManifest():', m.version); return; }
        else console.debug('[footer] chrome.runtime.getManifest() returned no version', m);
      }catch(e){ console.warn('[footer] chrome.runtime.getManifest() threw', e); /* ignore and fallback */ }
    } else {
      console.debug('[footer] chrome.runtime.getManifest not available, falling back to fetch');
    }

    // Fallback: fetch manifest.json relative to pages directory
    (async function(){
      try{
        console.debug('[footer] fetching ../manifest.json');
        const resp = await fetch('../manifest.json');
        if(!resp.ok) { console.warn('[footer] fetch ../manifest.json failed:', resp.status); return; }
        const m = await resp.json();
        const v = m && m.version ? m.version : '';
        if(v) { el.textContent = 'v' + v + ' '; console.debug('[footer] got version from ../manifest.json:', v); }
        else console.debug('[footer] ../manifest.json has no version field', m);
      }catch(e){ console.error('[footer] error fetching ../manifest.json', e); }
    })();
  }catch(e){ console.error('[footer] unexpected error while setting version', e); }
})();
