// regex_worker.js — run single RegExp against many lines and return first-match info
self.onmessage = function(e){
  const data = e.data || {};
  const regSource = data.regSource || '';
  const regFlags = data.regFlags || 'u';
  const lines = data.lines || [];
  try{
    const reg = new RegExp(regSource, regFlags);
    const out = [];
    for(let i=0;i<lines.length;i++){
      const li = lines[i];
      const line = li && li.text ? li.text : '';
      if(!line) continue;
      try{
        const m = reg.exec(line);
        if(m){ out.push({ lineIndex: i, match: m[0], index: m.index }); continue; }
      }catch(_e){ /* try norm below */ }
      const norm = li && li.norm ? li.norm : '';
      if(norm){
        try{
          const nm = reg.exec(norm);
          if(nm){
            const normIdx = nm.index;
            const map = li.map || [];
            const mapped = (map && map[normIdx] !== undefined) ? map[normIdx] : 0;
            out.push({ lineIndex: i, match: nm[0], index: mapped });
          }
        }catch(_e){}
      }
    }
    self.postMessage({ ok: true, matches: out });
  }catch(err){
    self.postMessage({ ok: false, error: String(err && err.message ? err.message : err) });
  }
};
