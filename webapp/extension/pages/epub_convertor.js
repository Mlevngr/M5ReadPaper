// epub_convertor.js — 将 .epub 转为纯文本 (.txt)，并可选导出简单 .idx 目录
document.addEventListener('DOMContentLoaded', () => {
  const el = id => document.getElementById(id);
  const fileInput = el('fileInput');
  const dropArea = el('dropArea');
  const clearBtn = el('clearList');
  const generateBtn = el('generateBtn');
  const cancelBtn = el('cancelBtn');
  const progressBar = el('progressBar');
  const logEl = el('log');
  const convertInfo = el('convert-info');
  const downloadArea = el('downloadArea');
  const downloadAllZipBtn = el('downloadAllZip');
  const clearGeneratedBtn = el('clearGenerated');
  const optExtractToc = el('opt_extract_toc');
  const optDebug = el('opt_debug_details');
  const optRemoveBlankLines = el('opt_remove_blank_lines');

  const files = [];
  let running = false;
  let abortRequested = false;

  function log(msg) { const now = new Date().toLocaleTimeString(); logEl.textContent += `[${now}] ${msg}\n`; logEl.scrollTop = logEl.scrollHeight; }
  function dlog(msg) { if (optDebug && optDebug.checked) { log(msg); } }

  // 控制：仅当用户勾选“导出 idx”时才显示“显示目录检测详情”选项
  function updateDebugVisibility() {
    try {
      if (!optDebug) return;
      const show = !!(optExtractToc && optExtractToc.checked);
      // 优先尝试找到最近的 .field 容器来控制整行显示；若不存在则只控制 checkbox 本体
      let fieldContainer = null;
      if (typeof optDebug.closest === 'function') {
        fieldContainer = optDebug.closest('.field');
      } else {
        // 向上查找最多 4 层以防古怪的结构
        let p = optDebug.parentElement; let depth = 0;
        while (p && depth < 6) { if (p.classList && p.classList.contains('field')) { fieldContainer = p; break; } p = p.parentElement; depth++; }
      }
      if (fieldContainer) {
        fieldContainer.style.display = show ? '' : 'none';
      } else {
        optDebug.style.display = show ? '' : 'none';
      }
      // 若被隐藏，确保其状态为未勾选，避免隐藏时仍保留旧状态
      if (!show) optDebug.checked = false;
    } catch (e) { /* noop */ }
  }
  // 初始化并绑定事件
  try { updateDebugVisibility(); if (optExtractToc) optExtractToc.addEventListener('change', updateDebugVisibility); } catch (e) { }
  function setProgress(p) { try { progressBar.value = Math.max(0, Math.min(100, Math.round(p))); } catch (e) { } }

  function updateButtons() {
    if (generateBtn) generateBtn.disabled = running || files.length === 0;
    if (cancelBtn) cancelBtn.disabled = !running;
    if (clearBtn) clearBtn.disabled = running || files.length === 0;
    if (clearGeneratedBtn) clearGeneratedBtn.disabled = running;
    if (fileInput) fileInput.disabled = running;
    if (dropArea) {
      dropArea.classList.toggle('disabled', running);
    }
  }

  // generated files storage: [{ base: 'name', outputs: [{name, blob, size, type}] }]
  const generated = [];

  function renderDownloadArea() {
    if (!downloadArea) return;
    downloadArea.innerHTML = '';
    downloadArea.className = 'row';
    if (generated.length === 0) {
      downloadArea.innerHTML = '<div class="muted">暂无生成文件</div>';
      if (downloadAllZipBtn) downloadAllZipBtn.disabled = true;
      return;
    }
    if (downloadAllZipBtn) downloadAllZipBtn.disabled = false;
    for (const g of generated) {
      const box = document.createElement('div');
      box.className = 'panel col-12';
      const header = document.createElement('div');
      header.style.display = 'flex'; header.style.justifyContent = 'space-between'; header.style.alignItems = 'center';
      // Row 1: title (col-7) + actions (col-4)
      const row1 = document.createElement('div'); row1.className = 'download-row row align-center';
      const titleCol = document.createElement('div'); titleCol.className = 'col-7';
      const titleStrong = document.createElement('strong'); titleStrong.className = 'book-list-name'; titleStrong.textContent = g.base;
      titleCol.appendChild(titleStrong);
      const actionsCol = document.createElement('div'); actionsCol.className = 'col-4 small-button';
      const zipBtn = document.createElement('button'); zipBtn.className = 'button clear'; zipBtn.textContent = '打包下载';
      zipBtn.onclick = async () => {
        try {
          const JSZipLib = await ensureJSZipLoaded();
          const zip = new JSZipLib();
          for (const f of g.outputs) zip.file(f.name, f.blob);
          const zblob = await zip.generateAsync({ type: 'blob' });
          const url = URL.createObjectURL(zblob);
          const a = document.createElement('a'); a.href = url; a.download = g.base + '.zip'; a.classList.add('download-link'); document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 5000);
        } catch (e) {
          alert('打包失败: ' + (e && e.message));
        }
      };
      actionsCol.appendChild(zipBtn);
      row1.appendChild(titleCol);
      row1.appendChild(actionsCol);
      box.appendChild(row1);

      // Row 2: individual download links (txt / idx)
      const row2 = document.createElement('div'); row2.className = 'row small-button-row'; row2.style.marginTop = '8px';
      const txtCol = document.createElement('div'); txtCol.className = 'col-4 small-button';
      const idxCol = document.createElement('div'); idxCol.className = 'col-4 small-button';
      const imgCol = document.createElement('div'); imgCol.className = 'col-4 small-button';
      for (const f of g.outputs) {
        const url = URL.createObjectURL(f.blob);
        const a = document.createElement('a'); a.href = url; a.download = f.name; a.className="success";  a.style.marginRight = '8px';
        if (f.type === 'text') {
          a.textContent = 'txt下载';
          txtCol.appendChild(a);
        } else if (f.type === 'idx') {
          a.textContent = 'idx下载';
          idxCol.appendChild(a);
        } else if (f.type === 'image' || (f.name && /cover|封面/i.test(f.name))) {
          a.textContent = '封面下载';
          imgCol.appendChild(a);
        } else {
          // fallback: show generic download label
          a.textContent = f.name + ' (' + fmtSize(f.size) + ')';
          txtCol.appendChild(a);
        }
        a.onclick = () => setTimeout(() => URL.revokeObjectURL(url), 5000);
      }
      row2.appendChild(txtCol);
      row2.appendChild(idxCol);
      row2.appendChild(imgCol);
      box.appendChild(row2);
      downloadArea.appendChild(box);
    }
  }

  if (clearGeneratedBtn) clearGeneratedBtn.onclick = () => {
    // revoke URLs created earlier by render (they are revoked on click), but clear list
    generated.length = 0; renderDownloadArea();
  };

    if (downloadAllZipBtn) downloadAllZipBtn.onclick = async () => {
    if (generated.length === 0) return;
    try {
      const JSZipLib = await ensureJSZipLoaded();
      const zip = new JSZipLib();
      // Put all generated files at the zip root. Prefix filenames with book base to avoid name collisions.
      for (const g of generated) {
        const safeBase = (g.base || '').replace(/[\/]/g, '_');
        for (const f of g.outputs) {
          // Use the original filename for zip root entries (per user request)
          const entryName = f.name;
          zip.file(entryName, f.blob);
        }
      }
      const zblob = await zip.generateAsync({ type: 'blob' });
      const url = URL.createObjectURL(zblob);
      const a = document.createElement('a'); a.href = url; a.download = 'epub-convert-results.zip'; a.classList.add('download-link'); document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 5000);
    } catch (e) { alert('全部打包失败: ' + (e && e.message)); }
  };

  function addFilesFromList(list) {
    if (!Array.isArray(list) || !list.length) return;
    let added = 0;
    for (const file of list) {
      if (!file) continue;
      const duplicate = files.some(item => item.file.name === file.name && item.file.size === file.size && item.file.lastModified === file.lastModified);
      if (duplicate) {
        log(`跳过重复文件: ${file.name}`);
        continue;
      }
      const item = { file, status: '排队中', statusType: 'pending', progress: 0, statusEl: null, progressEl: null };
      files.push(item);
      added++;
    }
    if (added) {
      log(`已添加 ${added} 个文件到队列`);
      rebuildFileList();
      updateButtons();
    }
  }

  function rebuildFileList() {
    const fl = el('fileList');
    if (!fl) return;
    fl.innerHTML = '';
    files.forEach((item, index) => {
      const row = document.createElement('div');
      row.className = 'file-item col-12';
      row.innerHTML = `
        <div class="row align-center queue-row">
          <div class="col-7"><strong>${escapeHtml(item.file.name)}</strong> <span class="muted">${fmtSize(item.file.size)}</span></div>
          <div class="col-5 badge-item"><span class="status-badge" data-status="${item.statusType || 'pending'}">${item.status || '排队中'}</span></div>
          <div class="col-12 queue-progress">
            <progress max="100" value="${Math.round((item.progress || 0) * 100)}" class="progress-compact"></progress>
          </div>
        </div>`;
      item.statusEl = row.querySelector('.status-badge');
      item.progressEl = row.querySelector('progress');
      fl.appendChild(row);
      setStatus(item, item.status || '排队中', item.statusType || 'pending');
      setItemProgress(item, item.progress || 0);
    });
  }

  function setStatus(item, text, type) {
    if (!item) return;
    item.status = text;
    item.statusType = type || item.statusType || 'pending';
    if (item.statusEl) {
      item.statusEl.textContent = text;
      item.statusEl.setAttribute('data-status', item.statusType);
    }
  }

  function setItemProgress(item, frac) {
    if (!item) return;
    const clamped = Math.max(0, Math.min(1, frac || 0));
    item.progress = clamped;
    if (item.progressEl) {
      item.progressEl.value = Math.round(clamped * 100);
    }
  }

  function clearQueue() {
    if (running) {
      alert('当前正在转换，无法清空队列。请先取消或等待完成。');
      return;
    }
    files.length = 0;
    rebuildFileList();
    convertInfo.innerHTML = '';
    setProgress(0);
    log('已清空文件队列');
    updateButtons();
  }

  // drag & drop
  ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(ev => dropArea.addEventListener(ev, e => { e.preventDefault(); e.stopPropagation(); }));
  ['dragenter', 'dragover'].forEach(ev => dropArea.addEventListener(ev, () => dropArea.classList.add('dragover')));
  ['dragleave', 'drop'].forEach(ev => dropArea.addEventListener(ev, () => dropArea.classList.remove('dragover')));
  dropArea.addEventListener('drop', e => { const list = Array.from(e.dataTransfer.files || []); if (list.length) addFilesFromList(list); });

  fileInput.onchange = () => { const list = Array.from(fileInput.files || []); if (list.length) addFilesFromList(list); fileInput.value = ''; };
  clearBtn.onclick = clearQueue;

  function fmtSize(bytes) { if (bytes === 0) return '0B'; const u = ['B', 'KB', 'MB', 'GB']; const i = Math.floor(Math.log(bytes) / Math.log(1024)); return (bytes / Math.pow(1024, i)).toFixed(1) + u[i]; }
  function escapeHtml(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

  // dynamically load JSZip from CDN if not present
  function ensureJSZipLoaded() {
    return new Promise((resolve, reject) => {
      if (window.JSZip) return resolve(window.JSZip);
      // 优先尝试本地 vendors，如不存在则回退 CDN
      const candidates = [
        '../vendors/jszip.min.js',
        'https://cdn.jsdelivr.net/npm/jszip@3.10.1/dist/jszip.min.js'
      ];
      let idx = 0;
      function tryNext() {
        if (idx >= candidates.length) return reject(new Error('无法加载 JSZip'));
        const src = candidates[idx++];
        const s = document.createElement('script'); s.src = src; s.onload = () => {
          if (window.JSZip) resolve(window.JSZip); else tryNext();
        }; s.onerror = () => tryNext();
        document.head.appendChild(s);
      }
      tryNext();
    });
  }

  // helper: resolve href relative to a base path (both POSIX style)
  function resolvePath(base, href) {
    if (!base) return href;
    // base: path/to/file.opf  => baseDir = path/to/
    const idx = base.lastIndexOf('/');
    const baseDir = idx >= 0 ? base.slice(0, idx + 1) : '';
    if (href.startsWith('/')) return href.slice(1);
    return (baseDir + href).replace(/\\/g, '/');
  }

  // 根据 OPF 路径与原始 href 生成“规范化”路径，避免重复前缀 (如 OEBPS/OEBPS/xxx)；优先返回 zip 中真实存在的文件名
  function canonicalPath(baseOpf, href, zip) {
    const candidate1 = resolvePath(baseOpf, href); // 常规拼接
    // 如果 href 已经包含目录前缀（例如 OEBPS/text00001.html），再拼接会重复；尝试直接使用 href
    const candidate2 = href.replace(/^\/*/, '');
    if (zip) {
      if (zip.file(candidate1)) return candidate1;
      if (zip.file(candidate2)) return candidate2;
    }
    // fallback: 去重重复片段 OEBPS/OEBPS/ -> OEBPS/
    const dedup = candidate1.replace(/([^/]+)\/\1\//, '$1/');
    if (zip && zip.file(dedup)) return dedup;
    return candidate1; // 最后兜底
  }

  // 提取文本（保留 EPUB 的基本排版，不额外压缩），并可按需记录锚点 ID 的字节偏移（相对于本章节文本开头，UTF-8）
  async function extractTextFromXhtmlWithAnchors(xhtmlStr, idSet, zipObj, chapterPath) {
    try {
      const parser = new DOMParser();
      const doc = parser.parseFromString(xhtmlStr, 'text/html');
      // remove scripts
      Array.from(doc.querySelectorAll('script')).forEach(n => n.remove());

      // collect CSS: <style> + linked stylesheets (from zip)
      let collectedCss = '';
      try { Array.from(doc.querySelectorAll('style')).forEach(s => { collectedCss += (s.textContent || '') + '\n'; s.remove(); }); } catch (e) { }
      try {
        const links = Array.from(doc.querySelectorAll('link[rel="stylesheet"][href]'));
        for (const l of links) {
          const href = (l.getAttribute('href') || '').trim();
          l.remove();
          if (!href) continue;
          const resolved = resolvePath(chapterPath || '', href);
          try { if (zipObj && zipObj.file(resolved)) { const css = await zipObj.file(resolved).async('string'); collectedCss += css + '\n'; } } catch (e) { /* ignore */ }
        }
      } catch (e) { }

      // create off-screen container in current document so we can use getComputedStyle
      try {
        const container = document.createElement('div');
        container.style.position = 'fixed'; container.style.left = '-99999px'; container.style.top = '0';
        container.style.width = '1px'; container.style.height = '1px'; container.style.overflow = 'hidden'; container.style.pointerEvents = 'none';
        const styleEl = document.createElement('style'); styleEl.textContent = collectedCss; container.appendChild(styleEl);
        const clone = document.createElement('div');
        const bodyChildren = doc.body ? Array.from(doc.body.childNodes) : [];
        for (const n of bodyChildren) clone.appendChild(n.cloneNode(true));
        container.appendChild(clone);
        document.documentElement.appendChild(container);

        const enc = new TextEncoder();
        const outParts = [];
        let byteCount = 0;
        const anchorOffsets = new Map();
        // lightweight utf8 byte length calc (avoids TextEncoder allocation per small chunk)
        function utf8ByteLenOfString(str) {
          if (!str) return 0;
          let b = 0;
          for (const ch of str) {
            const cp = ch.codePointAt(0);
            if (cp <= 0x7F) b += 1;
            else if (cp <= 0x7FF) b += 2;
            else if (cp <= 0xFFFF) b += 3;
            else b += 4;
          }
          return b;
        }
        function appendText(s) { if (!s) return; outParts.push(s); byteCount += utf8ByteLenOfString(s); }

        const walker = document.createTreeWalker(clone, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT, null, false);
        let node;
        function isNodeHidden(el) {
          try {
            if (!el) return false;
            const tag = (el.tagName || '').toLowerCase();
            if (tag === 'template' || tag === 'noscript') return true;
            if (el.hasAttribute && el.hasAttribute('hidden')) return true;
            if ((el.getAttribute && (el.getAttribute('aria-hidden') || '')).toLowerCase() === 'true') return true;
            const cs = window.getComputedStyle(el);
            if (!cs) return false;
            if (cs.display === 'none' || cs.visibility === 'hidden' || parseFloat(cs.opacity) === 0) return true;
          } catch (e) { }
          return false;
        }

        const blockSet = new Set(['p','div','section','article','header','footer','main','aside','nav','h1','h2','h3','h4','h5','h6','li','dt','dd','pre','blockquote','figure','figcaption','table','thead','tbody','tfoot','tr']);

        // pendingAnchors: id -> { el, startByte }
        const pendingAnchors = new Map();

        let processedNodes = 0;
        while (node = walker.nextNode()) {
          processedNodes++;
          if ((processedNodes & 0x1FF) === 0) {
            // yield to event loop every 512 nodes to avoid long blocking
            await new Promise(r => setTimeout(r, 0));
          }
          if (node.nodeType === Node.TEXT_NODE) {
            const parent = node.parentElement;
            if (!parent) continue;
            if (isNodeHidden(parent)) continue;
            // If this visible text node is inside any pending anchor element,
            // record the anchor offset at the current byteCount (start of
            // this text chunk) — this yields the nearest visible-text start.
            try {
              let a = parent;
              while (a) {
                if (a.id && pendingAnchors.has(a.id) && !anchorOffsets.has(a.id)) {
                  anchorOffsets.set(a.id, byteCount);
                }
                a = a.parentElement;
              }
            } catch (e) { }
            const txt = (node.textContent || '').replace(/\s+/g, ' ');
            appendText(txt);
          } else if (node.nodeType === Node.ELEMENT_NODE) {
            const el = node;
            if (isNodeHidden(el)) { walker.currentNode = el; continue; }
            const tag = (el.tagName || '').toLowerCase();
            if (tag === 'br') { appendText('\n'); continue; }
            if (tag === 'hr') { appendText('\n\n'); continue; }
            if (el.id && (!idSet || idSet.has(el.id))) {
              // record as pending; if inner visible text exists it'll win,
              // otherwise we'll fall back to this startByte after walk.
              if (!anchorOffsets.has(el.id) && !pendingAnchors.has(el.id)) pendingAnchors.set(el.id, { el: el, startByte: byteCount });
            }
            if (tag === 'ul' || tag === 'ol') {
              const items = Array.from(el.children || []).filter(ch => ch.tagName && ch.tagName.toLowerCase() === 'li');
              for (let i = 0; i < items.length; i++) {
                const li = items[i];
                // include visible child text
                const texts = li.innerText || li.textContent || '';
                if (texts && !isNodeHidden(li)) appendText((texts || '').replace(/\s+/g,' '));
                if (i < items.length - 1) appendText('\n');
              }
              appendText('\n\n');
              walker.currentNode = el; // skip deeper walk as we've consumed
              continue;
            }
            if (blockSet.has(tag)) {
              const last = outParts.length ? outParts[outParts.length - 1] : '';
              if (!/\n\n$/.test(last)) {
                if (/\n$/.test(last)) appendText('\n'); else appendText('\n\n');
              }
            }
          }
        }

        // finalize pending anchors: those without interior visible text fall
        // back to their element startByte recorded earlier.
        try {
          for (const [id, info] of pendingAnchors.entries()) {
            if (!anchorOffsets.has(id)) anchorOffsets.set(id, info.startByte || 0);
          }
        } catch (e) { }

        document.documentElement.removeChild(container);
        let out = outParts.join('');
        out = out.replace(/\r\n/g,'\n').replace(/(\n\s*){3,}/g,'\n\n');
        return { text: out.trim(), anchors: anchorOffsets };
      } catch (e) {
        return { text: '', anchors: new Map() };
      }
    } catch (e) { return { text: '', anchors: new Map() }; }
  }

  // 解析 OPF，优先 nav.xhtml，回落 toc.ncx，返回目录项：[{title, hrefPath, fragment}]
  async function extractTocEntries(zip, pkgDoc, baseOpf) {
    // build manifest lookup by id and by href
    const manifest = {};
    pkgDoc.querySelectorAll('manifest > item').forEach(it => {
      const id = it.getAttribute('id'); const href = it.getAttribute('href') || ''; const media = it.getAttribute('media-type') || ''; const props = (it.getAttribute('properties') || '').split(/\s+/).filter(Boolean);
      manifest[id] = { id, href, media, props };
    });

    // 1) nav.xhtml (EPUB3)
    let navItem = null;
    for (const id in manifest) { const it = manifest[id]; if (it.props && it.props.includes('nav')) { navItem = it; break; } }
    if (navItem) {
      try {
        const navPath = resolvePath(baseOpf, navItem.href);
        const navFile = zip.file(navPath);
        if (navFile) {
          const navStr = await navFile.async('string');
          const parser = new DOMParser();
          const navDoc = parser.parseFromString(navStr, 'text/html');
          // find <nav epub:type="toc"> or role=doc-toc or first <nav>
          let navEl = navDoc.querySelector('nav[epub\:type~="toc"], nav[role="doc-toc"]');
          if (!navEl) navEl = navDoc.querySelector('nav');
          const list = [];
          if (navEl) {
            navEl.querySelectorAll('a[href]').forEach(a => {
              const href = a.getAttribute('href') || ''; const title = (a.textContent || '').trim();
              if (!href || !title) return;
              const resolved = resolvePath(navPath, href);
              const [p, frag] = resolved.split('#');
              list.push({ title, hrefPath: p, fragment: frag || null });
            });
          }
          if (list.length) return list;
        }
      } catch (e) { /* ignore, fallback to ncx */ }
    }

    // 2) toc.ncx (EPUB2)
    try {
      const spine = pkgDoc.querySelector('spine');
      const tocId = spine ? spine.getAttribute('toc') : null;
      if (tocId && manifest[tocId]) {
        const ncxPath = resolvePath(baseOpf, manifest[tocId].href);
        const ncxFile = zip.file(ncxPath);
        if (ncxFile) {
          const ncxStr = await ncxFile.async('string');
          const parser = new DOMParser();
          const ncxDoc = parser.parseFromString(ncxStr, 'application/xml');
          const list = [];
          function walkNavPoint(el) {
            el.querySelectorAll(':scope > navPoint').forEach(np => {
              const label = np.querySelector('navLabel > text');
              const content = np.querySelector('content');
              const title = label ? (label.textContent || '').trim() : '';
              const src = content ? (content.getAttribute('src') || '') : '';
              if (src && title) {
                const resolved = resolvePath(ncxPath, src);
                const [p, frag] = resolved.split('#');
                list.push({ title, hrefPath: p, fragment: frag || null });
              }
              walkNavPoint(np);
            });
          }
          const root = ncxDoc.querySelector('navMap'); if (root) walkNavPoint(root);
          if (list.length) return list;
        }
      }
    } catch (e) { /* ignore */ }

    return [];
  }

  // convert EPUB buffer to text: returns { text, idxEntries }
  async function epubToText(arrayBuffer, onProgress) {
    const JSZipLib = await ensureJSZipLoaded();
    const zip = await JSZipLib.loadAsync(arrayBuffer);
    // locate container.xml
    const containerPath = 'META-INF/container.xml';
    if (!zip.file(containerPath)) throw new Error('未找到 ' + containerPath + ' (可能不是标准 EPUB 2/3)');
    const containerStr = await zip.file(containerPath).async('string');
    const parser = new DOMParser();
    const doc = parser.parseFromString(containerStr, 'application/xml');
    const rootfileEl = doc.querySelector('rootfile');
    if (!rootfileEl) throw new Error('container.xml 中未找到 rootfile');
    const fullPath = rootfileEl.getAttribute('full-path'); if (!fullPath) throw new Error('rootfile missing full-path');

    // read package (OPF)
    if (!zip.file(fullPath)) throw new Error('无法找到 package (OPF) 文件: ' + fullPath);
    const packageStr = await zip.file(fullPath).async('string');
    const pkgDoc = parser.parseFromString(packageStr, 'application/xml');

    // build manifest map id->href, and spine list of ids
    const manifest = {}; const manifestEls = pkgDoc.querySelectorAll('manifest > item');
    manifestEls.forEach(it => { const id = it.getAttribute('id'); const href = it.getAttribute('href'); const media = it.getAttribute('media-type'); const props = (it.getAttribute('properties') || ''); manifest[id] = { href, media, props }; });
    const spineIds = []; const spineEls = pkgDoc.querySelectorAll('spine > itemref'); spineEls.forEach(it => { const idref = it.getAttribute('idref'); if (idref) spineIds.push(idref); });

    // if spine empty, try to fallback to reading all html/xhtml files in manifest in order
    const itemsToRead = (spineIds.length > 0) ? spineIds.map(id => ({ id, ...manifest[id] })).filter(Boolean) : Array.from(Object.entries(manifest)).filter(([k, m]) => m.media && /x?html|xml|application\/xhtml\+xml|text\/html/i.test(m.media || '')).map(([id, m]) => ({ id, ...m }));

    // 调试日志：显示 spine 和待读文件信息
    dlog(`[EPUB 解析] spine 文件数: ${spineIds.length}, 待读文件数: ${itemsToRead.length}`);
    dlog(`[EPUB 解析] spine IDs: ${spineIds.slice(0, 5).join(', ')}${spineIds.length > 5 ? '...' : ''}`);
    dlog(`[EPUB 解析] 待读文件前5个: ${itemsToRead.slice(0, 5).map(it => it.href).join(', ')}`);
    
    const baseOpf = fullPath;
    // 解析 TOC（无论是否导出 idx，都用于判定目录页/目录块）
    const tocEntriesAll = await extractTocEntries(zip, pkgDoc, baseOpf);
    dlog('TOC 条目数: ' + tocEntriesAll.length);

    // 严谨目录页与目录块识别：
    // 思路：
    // 1. 官方 TOC 结构：存在 <nav epub:type="toc"> (EPUB3) 或 NCX 入口标题为 "目录"/"Contents" 等。
    // 2. 若某页主体几乎全部由 TOC 导航组成（nav 中链接文本占比高、非导航正文极少），标记为“整页目录”跳过。
    // 3. 若页面既包含 TOC 导航又有大量正文，则仅在提取前移除 TOC 导航节点，不整页跳过。
    // 4. 不再仅依赖文件名；基于内容结构与链接密度。
    const standaloneTocFiles = new Set(); // 完全跳过的目录页
    const stripNavPages = new Set(); // 需要剥离导航块的页面
    async function analyzeTocStructure() {
      // 多语言目录标题关键字（用于辅助识别 nav.xhtml 自身是否是目录页）
      const titleKeywords = /^(目录|目錄|目次|contents?|table\s*of\s*contents?|sommaire|indice|índice)$/i;
      // 预先构建 href 去重集合（去掉 fragment）
      const hrefSet = new Set(tocEntriesAll.map(e => canonicalPath(baseOpf, e.hrefPath, zip)));
      // 章节标题集合（用于纯文本目录页判定：大量行与标题集合匹配）
      const allTocTitlesLower = tocEntriesAll.map(e => (e.title || '').trim().toLowerCase()).filter(Boolean);
      const tocTitleSet = new Set(allTocTitlesLower);
      // spine 目标集合（用于统计链接是否主要指向书内章节）
      const spineHrefSet = new Set(itemsToRead.map(it => canonicalPath(baseOpf, it.href, zip)));
      // 遍历所有待读 items，判定其是否包含 TOC 导航
      for (const item of itemsToRead) {
        if (!item || !item.href) continue;
        const hrefPath = canonicalPath(baseOpf, item.href, zip);
        const fileObj = zip.file(hrefPath);
        if (!fileObj) continue;
        let html;
        try { html = await fileObj.async('string'); } catch (e) { continue; }
        // DOM 解析以进行精细判断
        let doc;
        try {
          const parser = new DOMParser();
          doc = parser.parseFromString(html, 'text/html');
        } catch (e) { continue; }
        const navNodes = Array.from(doc.querySelectorAll('nav[epub\\:type~="toc"], nav[role="doc-toc"], nav#toc, nav.toc'));
        // 识别“目录”标题（非 nav），例如 <div class="contents-title">目录</div>、heading 中的目录、或开头短文本“目录”
        const headingTitleEl = doc.querySelector('h1,h2,h3,h4,h5,h6,.contents-title,.toc-title,#toc-title');
        const headingTitleText = headingTitleEl ? (headingTitleEl.textContent || '').trim() : '';
        let forcedDirectoryTitle = titleKeywords.test(headingTitleText);
        if (!forcedDirectoryTitle) {
          const bodyHead = (doc.body ? (doc.body.innerText || '') : (doc.documentElement.innerText || ''))
            .replace(/\s+/g, ' ')  // 压缩空白，避免换行影响
            .trim()
            .slice(0, 120);
          if (titleKeywords.test(bodyHead)) forcedDirectoryTitle = true;
        }
        // 额外：若该页出现在 tocEntries 的链接集合中，且对应标题就是“目录”，也视为目录页
        if (!forcedDirectoryTitle) {
          forcedDirectoryTitle = tocEntriesAll.some(e => canonicalPath(baseOpf, e.hrefPath, zip) === hrefPath && titleKeywords.test((e.title || '').trim()));
        }
        // 统计链接与正文占比（若无 navNodes 且 forcedDirectoryTitle，则使用全页链接统计）
        const linkTexts = [];
        let navLinkCount = 0; // 导航/目录链接数量
        let navTextLen = 0;   // 导航区域文字总长度
        if (navNodes.length > 0) {
          for (const nav of navNodes) {
            const links = Array.from(nav.querySelectorAll('a[href]'));
            navLinkCount += links.length;
            links.forEach(a => linkTexts.push((a.textContent || '').trim()));
            navTextLen += (nav.innerText || '').replace(/\s+/g, ' ').trim().length;
          }
        } else {
          // 无显式 nav：统计整页所有指向其它 spine 项的链接
          const allLinks = Array.from(doc.querySelectorAll('a[href]'));
          for (const a of allLinks) {
            const raw = (a.getAttribute('href') || '').replace(/#.*$/, '');
            const resolved = canonicalPath(baseOpf, resolvePath(hrefPath, raw), zip);
            if (spineHrefSet.has(resolved)) {
              navLinkCount++;
              linkTexts.push((a.textContent || '').trim());
            }
          }
          // 目录文本粗略长度：链接文本拼接 + 若存在标题“目录”为其增加少量权重
          navTextLen = linkTexts.join(' ').replace(/\s+/g, ' ').trim().length + (forcedDirectoryTitle ? 2 : 0);
        }
        const bodyText = (doc.body ? doc.body.innerText : doc.documentElement.innerText || '').replace(/\s+/g, ' ').trim();
        const bodyLen = bodyText.length;
        // 非导航正文（粗略）
        const nonNavLen = Math.max(0, bodyLen - navTextLen);
        // 判定策略：
        // 整页目录条件：
        //  (A) 有 navNodes: 导航链接 >=10 且 导航文字占比>=0.6 且 非导航正文 < 800
        //  (B) 无 navNodes: 若存在“目录”标题或 .contents-title/.toc-title，且链接指向 spine 的数量>=5，且 (navTextLen/bodyLen)>=0.5 且 nonNavLen < 800
        //  (C) 结构提示：同时存在 .contents-title 且 .contents1/.contents2 中的链接总数>=3，也视为整页目录
        const isStandalone = navTextLen > 0 && (
          (navNodes.length > 0 && navLinkCount >= 10 && (navTextLen / bodyLen) >= 0.6 && nonNavLen < 800) ||
          (navNodes.length === 0 && (forcedDirectoryTitle || doc.querySelector('.contents-title,.toc-title')) && navLinkCount >= 5 && (navTextLen / bodyLen) >= 0.5 && nonNavLen < 800) ||
          (navNodes.length === 0 && doc.querySelector('.contents-title') && doc.querySelectorAll('.contents1 a, .contents2 a').length >= 3)
        );
        if (isStandalone) {
          standaloneTocFiles.add(hrefPath);
          dlog('识别整页目录: ' + hrefPath + ' (links:' + navLinkCount + ', nav占比:' + (navTextLen / bodyLen).toFixed(2) + ', nonNav:' + nonNavLen + ')');
          continue;
        }
        // 纯文本目录页补充判定：无/极少链接但含目录标题，且前若干行大部分都匹配章节标题集合
        if (!isStandalone && forcedDirectoryTitle && navLinkCount < 3) {
          const rawLines = (doc.body ? doc.body.innerText : bodyText).split(/\r?\n/).map(l => l.trim()).filter(l => l.length > 0);
          const sampleLines = rawLines.slice(0, 120); // 前 120 行采样
          let matchCount = 0;
          for (const line of sampleLines) {
            const norm = line.toLowerCase().replace(/\s+/g, ' ');
            if (tocTitleSet.has(norm)) matchCount++;
          }
          const density = sampleLines.length ? (matchCount / sampleLines.length) : 0;
          // 若匹配密度 >= 0.5 且 章节标题匹配数 >= 5 且 正文总长度不大（避免误杀章节合集）
          if (matchCount >= 5 && density >= 0.5 && bodyLen < 15000) {
            standaloneTocFiles.add(hrefPath);
            dlog('识别纯文本目录页(密度): ' + hrefPath + ' (match:' + matchCount + ', density:' + density.toFixed(2) + ')');
            continue;
          }
        }
        // 混合页条件：
        //  (有 navNodes && 导航链接>=5) 或 (forcedDirectoryTitle && navLinkCount>=5)
        if ((navNodes.length > 0 && navLinkCount >= 5) || (navNodes.length === 0 && forcedDirectoryTitle && navLinkCount >= 5)) {
          stripNavPages.add(hrefPath);
          dlog('识别混合目录页(剥离导航/标题): ' + hrefPath + ' (links:' + navLinkCount + ')');
        }
      }
    }
    await analyzeTocStructure();
    dlog('整页目录文件数: ' + standaloneTocFiles.size + '；混合页数: ' + stripNavPages.size);
    function stripTocNav(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, 'text/html');
        // 1) 移除所有具有 toc 语义的节点（不限于 nav）
        doc.querySelectorAll('[epub\\:type~="toc"], [role="doc-toc"], #toc, .toc').forEach(n => n.remove());
        // 2) 若存在以“目录/Contents/Table of contents”等为标题的 heading，移除该标题及紧随其后的链接列表块
        const titleKeywordsRe = /^(目录|目錄|目次|contents?|table\s*of\s*contents?|sommaire|indice|índice)$/i;
        const headings = Array.from(doc.querySelectorAll('h1,h2,h3,h4,h5,h6'))
          .filter(h => titleKeywordsRe.test((h.textContent || '').trim().toLowerCase()));
        function linkCount(el) { try { return el ? el.querySelectorAll('a[href]').length : 0; } catch (e) { return 0; } }
        for (const h of headings) {
          let sib = h.nextElementSibling;
          h.remove();
          let steps = 0;
          while (sib && steps < 30) {
            const tag = sib.tagName ? sib.tagName.toLowerCase() : '';
            const lcnt = linkCount(sib);
            const textLen = (sib.innerText || '').replace(/\s+/g, ' ').trim().length;
            const isTocBlock = (
              tag === 'ul' || tag === 'ol' || tag === 'nav' ||
              (tag === 'div' && lcnt >= 3) ||
              (tag === 'section' && lcnt >= 3) ||
              (tag === 'p' && lcnt >= 1 && textLen <= 80) ||
              (tag === 'li' && lcnt >= 1 && textLen <= 80)
            );
            if (isTocBlock) {
              const toRemove = sib; sib = sib.nextElementSibling; toRemove.remove(); steps++; continue;
            }
            break;
          }
        }
        return doc.documentElement ? doc.documentElement.outerHTML : html;
      } catch (e) { return html; }
    }
    // group toc targets by chapter path
    const targetsByPath = new Map();
    for (const e of tocEntriesAll) {
      const key = canonicalPath(baseOpf, e.hrefPath, zip);
      if (!targetsByPath.has(key)) targetsByPath.set(key, []);
      targetsByPath.get(key).push(e);
    }

    // 调试日志：汇总目录识别结果
    dlog(`[目录识别] 整页目录文件数: ${standaloneTocFiles.size}`);
    dlog(`[目录识别] 需剥离导航的文件数: ${stripNavPages.size}`);
    if (standaloneTocFiles.size > 0) {
      dlog(`[目录识别] 整页目录列表: ${Array.from(standaloneTocFiles).join(', ')}`);
    }
    if (stripNavPages.size > 0) {
      dlog(`[目录识别] 需剥离导航列表: ${Array.from(stripNavPages).join(', ')}`);
    }

    let total = itemsToRead.length; let processed = 0; let outTextParts = [];
    const encoder = new TextEncoder();
    let cumulativeBytes = 0; // total bytes so far of output txt
    const segments = []; // {text, bytes}
    const chapterStartBytes = new Map(); // resolvedPath -> startByte
    const processedPaths = []; // order-aligned list of hrefs for segments
    const anchorAbsBytes = new Map(); // key: resolvedPath#fragment -> absolute byte position

    // Prepare per-segment post-processor if requested (so we can compute chapter start
    // bytes based on the processed segment bytes directly). This avoids needing a global
    // byteOffsetMapper for whole-document mapping and reduces mapping errors.
    const shouldRemoveBlankLines = optRemoveBlankLines && optRemoveBlankLines.checked;
    let perSegmentPostProcessor = null;
    let aggregatedPostProcessStats = {};
    if (typeof window.createTextPostProcessor === 'function' && shouldRemoveBlankLines) {
      try {
        perSegmentPostProcessor = window.createTextPostProcessor({ removeBlankLines: true });
      } catch (e) {
        dlog('[EPUBDbg] 无法创建文本后处理器: ' + (e && e.message));
        perSegmentPostProcessor = null;
      }
    }

    for (const item of itemsToRead) {
      if (abortRequested) throw new Error('已取消');
      const href = canonicalPath(baseOpf, item.href, zip);
      // 调试日志：记录每个文件的处理状态
      dlog(`[文件处理] ${processed + 1}/${total}: ${href}`);
      dlog(`  - 是否为整页目录: ${standaloneTocFiles.has(href)}`);
      dlog(`  - 是否需要剥离导航: ${stripNavPages.has(href)}`);
      
      // 不再提前跳过整页目录；统一先拼接，再在最终文本阶段裁剪目录前缀
      let fileEntry = zip.file(href);
      if (!fileEntry) { // try without base path as fallback
        log('警告: 未在 zip 中找到 ' + href + ', 尝试查找同名文件');
        dlog(`[文件处理] ❌ 文件不存在于 ZIP: ${href}`);
        // try find by filename only
        const nameOnly = href.split('/').pop();
        const guess = Object.keys(zip.files).find(k => k.endsWith('/' + nameOnly) || k === nameOnly);
        if (guess) {
          fileEntry = zip.file(guess);
          dlog(`[文件处理] ✓ 找到替代路径: ${guess}`);
        }
      }
      if (!fileEntry) { 
        log('跳过: 无法找到 ' + href); 
        dlog(`[文件处理] ⛔ 跳过文件（找不到）: ${href}`);
        processed++; setProgress((processed / total) * 100); continue; 
      }
      try {
        let s = await fileEntry.async('string');
        dlog(`[文件处理] ✓ 读取成功，大小: ${s.length} 字符`);
        
        if (stripNavPages.has(href)) {
          dlog(`[文件处理] 🔧 剥离导航块...`);
          s = stripTocNav(s);
        }
        // prepare id set for this chapter if we have toc targets pointing inside
        const chTargets = targetsByPath.get(href) || [];
        const idSet = new Set(chTargets.filter(t => t.fragment).map(t => t.fragment));
        let res = await extractTextFromXhtmlWithAnchors(s, idSet, zip, href);
        let { text: chapterText, anchors } = res || { text: '', anchors: new Map() };
        dlog(`[文件处理] ✓ 提取文本: ${chapterText.length} 字符, 前100字: ${chapterText.slice(0, 100).replace(/\n/g, '\\n')}...`);
        // 目录与正文混页的安全兜底：如果章节文本以“目录/Contents…”标题开头，剥掉这些头部行
        if (stripNavPages.has(href)) {
          const headStripRe = /^(\s*(目录|目錄|目次|contents?|table\s*of\s*contents?|sommaire|indice|índice)\s*(\r?\n|\n))+\s*/i;
          chapterText = chapterText.replace(headStripRe, '').trimStart();
          // 进一步：移除开头连续若干行若这些行恰好是 TOC 标题集合的一部分（通常是目录列出的章节标题）
          const lines = chapterText.split(/\r?\n/);
          const tocTitleSet = new Set((tocEntriesAll || []).map(e => (e.title || '').trim().toLowerCase()).filter(Boolean));
          let cut = 0;
          for (let i = 0; i < Math.min(lines.length, 100); i++) {
            const t = lines[i].trim().toLowerCase();
            if (!t) { cut++; continue; }
            if (tocTitleSet.has(t)) { cut++; continue; }
            break;
          }
          if (cut > 0) {
            chapterText = lines.slice(cut).join('\n').trimStart();
          }
        }
        // Build current segment and optionally post-process it per-segment so that
        // chapterStartBytes and anchor positions are based on the processed bytes.
        const sep = (chapterText && !chapterText.endsWith('\n\n')) ? '\n\n' : '';
        const segmentText = chapterText + sep;

        let processedSegment = segmentText;
        let localMapFunc = null;
        if (perSegmentPostProcessor) {
          try {
            const pr = perSegmentPostProcessor.process(segmentText);
            processedSegment = pr.text;
            localMapFunc = pr.byteOffsetMap;
            // aggregate stats
            if (pr.stats) {
              for (const k of Object.keys(pr.stats)) {
                aggregatedPostProcessStats[k] = aggregatedPostProcessStats[k] || { linesRemoved: 0, bytesRemoved: 0 };
                const s = pr.stats[k];
                if (s && typeof s.linesRemoved === 'number') aggregatedPostProcessStats[k].linesRemoved += s.linesRemoved || 0;
                if (s && typeof s.bytesRemoved === 'number') aggregatedPostProcessStats[k].bytesRemoved += s.bytesRemoved || 0;
              }
            }
          } catch (e) { dlog('[EPUBDbg] per-segment postProcessor failed: ' + (e && e.message)); processedSegment = segmentText; localMapFunc = null; }
        }

        // record chapter start byte (based on already-processed prior segments)
        chapterStartBytes.set(href, cumulativeBytes);
        try { dlog(`[EPUBDbg] chapterStartBytes ${href} = ${cumulativeBytes}`); } catch (e) { }

        // push processed segment
        outTextParts.push(processedSegment);
        processedPaths.push(href);
        const bytesAdded = encoder.encode(processedSegment).length;
        segments.push({ text: processedSegment, bytes: bytesAdded });
        dlog(`[文件处理] ✅ 添加到输出: ${href}, 字节数: ${bytesAdded}`);
        try {
          const preview = (processedSegment && processedSegment.length > 200) ? processedSegment.slice(0, 200).replace(/\n/g, '\\n') + '...' : (processedSegment || '').replace(/\n/g, '\\n');
          dlog(`[EPUBDbg] ${href} bytesAdded=${bytesAdded}, preview='${preview}'`);
        } catch (e) { }

        // record anchors for this chapter (absolute byte) — map using localMapFunc if available
        if (chTargets.length) {
          for (const t of chTargets) {
            const key = t.fragment ? `${href}#${t.fragment}` : `${href}`;
            if (t.fragment && anchors.has(t.fragment)) {
              const oldOff = anchors.get(t.fragment) || 0;
              const mapped = (localMapFunc && typeof localMapFunc === 'function') ? localMapFunc(oldOff) : oldOff;
              anchorAbsBytes.set(key, cumulativeBytes + mapped);
            } else {
              // 若 fragment 未在锚集合中：尝试在原（或处理后）章节文本中匹配一次标题出现的位置（优先在 processedSegment 中查找）
              if (t.fragment) {
                const titleLower = (t.title || '').trim().toLowerCase();
                let pos = -1;
                if (titleLower) { pos = processedSegment.toLowerCase().indexOf(titleLower); }
                if (pos >= 0) {
                  const bytesBefore = encoder.encode(processedSegment.slice(0, pos)).length;
                  anchorAbsBytes.set(key, cumulativeBytes + bytesBefore);
                  continue;
                }
              }
              anchorAbsBytes.set(key, cumulativeBytes); // fallback to chapter start
            }
          }
        }
        cumulativeBytes += bytesAdded;
      } catch (e) { 
        log('读取条目失败: ' + (item.href || '') + ' -> ' + (e && e.message)); 
        dlog(`[文件处理] ❌ 异常: ${href} - ${e && e.message}\n${e && e.stack}`);
      }
      processed++; if (onProgress) onProgress(processed / total); setProgress((processed / total) * 100);
    }
    // finalize output (initial, still包含目录前缀)
    let finalText = outTextParts.join('');

    // Recompute authoritative chapter start positions based on the actual
    // segments we've appended (prefix sums). This catches any cases where
    // chapterStartBytes were recorded earlier but segments were later modified
    // (或被后处理改变长度)，causing idx位置偏移。
    try {
      const recomputed = new Map();
      let acc = 0;
      for (let i = 0; i < segments.length; i++) {
        const href = processedPaths[i] || null;
        recomputed.set(href, acc);
        acc += segments[i].bytes;
      }
      // Compare and patch chapterStartBytes and anchorAbsBytes
      for (const [href, newStart] of recomputed.entries()) {
        const old = chapterStartBytes.has(href) ? chapterStartBytes.get(href) : null;
        if (old === null || old === undefined) continue;
        if (old !== newStart) {
          const delta = newStart - old;
          dlog(`[EPUBDbg] Recomputed chapter start for ${href}: old=${old} new=${newStart} delta=${delta}`);
          chapterStartBytes.set(href, newStart);
          // Shift any anchors that belong to this href
          for (const [k, v] of Array.from(anchorAbsBytes.entries())) {
            if (k === href || k.indexOf(href + '#') === 0) {
              anchorAbsBytes.set(k, v + delta);
              dlog(`[EPUBDbg] Shifted anchor ${k} by ${delta} -> ${v + delta}`);
            }
          }
        }
      }
    } catch (e) { dlog('[EPUBDbg] recompute chapter starts failed: ' + (e && e.message)); }

    // 基于 idx 策略：确定第一个“非目录”章节的起始字节位置，用于裁剪前缀目录文本
    // 规则：从 tocEntriesAll 中找到第一个非目录标题且不在 standaloneTocFiles 的项；其章节起点字节 chapterStartBytes 为正文起点
    const titleKeywordsRePre = /^(目录|目錄|目次|contents?|table\s*of\s*contents?|sommaire|indice|índice)$/i;
    let removalBytes = 0; // 需裁剪的前缀字节长度
    const shouldTrimPrefix = tocEntriesAll.length >= 3 || standaloneTocFiles.size > 0;
    dlog(`[前缀裁剪] TOC条目数: ${tocEntriesAll.length}, 整页目录数: ${standaloneTocFiles.size}, 是否执行裁剪: ${shouldTrimPrefix}`);
    
    if (shouldTrimPrefix) {
      for (const e of tocEntriesAll) {
        const resolved = canonicalPath(baseOpf, e.hrefPath, zip);
        const title = (e.title || '').trim();
        if (titleKeywordsRePre.test(title)) continue; // 跳过目录标题
        // 选择章节起点而非锚点位置，确保整个目录块都被裁掉
        const startByte = chapterStartBytes.get(resolved);
        if (startByte != null) { removalBytes = startByte; dlog('正文起点章节: ' + resolved + '；裁剪前缀字节: ' + startByte); break; }
      }
    } else {
      dlog('[前缀裁剪] 跳过裁剪：TOC 不完整或无整页目录，保留所有内容');
    }
    // 回退方案：若未从 TOC 中找到首个非目录章节，但已识别出整页目录文件，则以“第一个非整页目录的 spine 项”为正文起点
    if (removalBytes === 0 && standaloneTocFiles.size > 0) {
      for (const item of itemsToRead) {
        if (!item || !item.href) continue;
        const p = canonicalPath(baseOpf, item.href, zip);
        if (standaloneTocFiles.has(p)) { continue; }
        const sb = chapterStartBytes.get(p);
        if (sb != null) { removalBytes = sb; dlog('正文起点回退(整页目录)：' + p + '；裁剪前缀字节: ' + sb); break; }
      }
    }
    if (removalBytes > 0) {
      // 将字节偏移转换为字符索引
      function bytesToCharIndex(target) {
        if (target <= 0) return 0;
        let accBytes = 0, accChars = 0;
        for (const seg of segments) {
          if (accBytes + seg.bytes < target) {
            // 整段在目标字节之前
            accBytes += seg.bytes; accChars += seg.text.length; continue;
          }
          if (accBytes + seg.bytes === target) {
            // 正好落在段尾边界
            return accChars + seg.text.length;
          }
          // 需要在当前段内查找精确字符位置
          const remaining = target - accBytes; // 目标在本段内的字节偏移（>0 且 < seg.bytes）
          let localBytes = 0; let pos = 0;
          for (; pos < seg.text.length; pos++) {
            const b = encoder.encode(seg.text[pos]).length;
            // 如果加上当前字符就会超过 remaining，说明字符起点就在 pos，不应再前进（避免越界到下一个字符，导致丢首字）
            if (localBytes + b > remaining) break;
            localBytes += b;
          }
          return accChars + pos;
        }
        return finalText.length; // fallback（极端情况下）
      }
      const cutCharIndex = bytesToCharIndex(removalBytes);
      // 为避免误删首字符：如果切点之后的第一个可见字符属于字母/汉字且 removalBytes 正好等于某段起点，则不再额外 +1
      if (cutCharIndex > 0 && cutCharIndex <= finalText.length) {
        let sliced = finalText.slice(cutCharIndex);
        // 额外：如果切完章节起点后还紧跟一些空白（换行/空格/制表），这些也会在最终文本中被移除，必须把它们计入前缀裁剪字节，否则首章节偏移会出现 1/2 的残差。
        const leadingWsMatch = sliced.match(/^\s+/);
        if (leadingWsMatch) {
          const wsBytes = encoder.encode(leadingWsMatch[0]).length;
          removalBytes += wsBytes; // 把被去掉的空白也视为前缀的一部分
          sliced = sliced.slice(leadingWsMatch[0].length);
        }
        finalText = sliced;
        // 调整总字节数与后续 idx 偏移
        cumulativeBytes = cumulativeBytes - removalBytes;
        dlog('已执行前缀裁剪，字符索引: ' + cutCharIndex + '，累积裁剪字节(含后续空白): ' + removalBytes + '，剩余总字节: ' + cumulativeBytes);
      }
    }
    // If we already applied per-segment post-processing above, collect the
    // aggregated stats for meta and skip global processing. Otherwise, if a
    // post-processor exists but we didn't run per-segment, apply it to the full
    // document (legacy path).
    let postProcessStats = null;
    let byteOffsetMapper = null; // kept for backward compatibility (not used when per-segment)
    if (perSegmentPostProcessor) {
      // use aggregated stats collected during per-segment processing
      if (Object.keys(aggregatedPostProcessStats).length) postProcessStats = aggregatedPostProcessStats;
    } else {
      if (typeof window.createTextPostProcessor === 'function') {
        const shouldRemoveBlankLinesFull = optRemoveBlankLines && optRemoveBlankLines.checked;
        if (shouldRemoveBlankLinesFull) {
          dlog('开始全文后处理：去除空行 (fallback)');
          try {
            const postProcessor = window.createTextPostProcessor({ removeBlankLines: true });
            const postResult = postProcessor.process(finalText);
            const originalTextSize = new TextEncoder().encode(finalText).length;
            finalText = postResult.text;
            const processedTextSize = new TextEncoder().encode(finalText).length;
            byteOffsetMapper = postResult.byteOffsetMap;
            postProcessStats = postResult.stats;
            cumulativeBytes = processedTextSize;
            if (postProcessStats && postProcessStats.RemoveBlankLines) {
              const stats = postProcessStats.RemoveBlankLines;
              dlog(`空行去除完成：移除 ${stats.linesRemoved} 行，减少 ${stats.bytesRemoved} 字节 (${originalTextSize} → ${processedTextSize})`);
            }
          } catch (e) { dlog('[EPUBDbg] 全文后处理失败: ' + (e && e.message)); }
        }
      }
    }
    // Build idx based on toc entries and absolute byte positions
    // 需求：过滤掉整页目录 standaloneTocFiles 对应的条目，使“目录”不出现在 idx 中
    const idx = [];
    // Helper: cooperative builder for prefixBytes to avoid long blocking
    async function buildPrefixBytesAsync(text, prefixBytes, progressBase = 0, progressRange = 1) {
      const totalChars = text ? text.length : 0;
      const BATCH = 4000; // characters per batch
      let acc = 0;
      let idx = 0;
      for (let start = 0; start < totalChars; start += BATCH) {
        const end = Math.min(start + BATCH, totalChars);
        for (let i = start; i < end; i++) {
          const ch = text[i];
          const cp = ch.codePointAt(0);
          let b = 0;
          if (cp <= 0x7F) b = 1;
          else if (cp <= 0x7FF) b = 2;
          else if (cp <= 0xFFFF) b = 3;
          else b = 4;
          acc += b;
          idx++;
          prefixBytes[idx] = acc;
        }
        // yield to UI
        await new Promise(r => setTimeout(r, 0));
        try {
          if (typeof setProgress === 'function') {
            const pct = progressBase + (progressRange * (start / Math.max(1, totalChars)));
            setProgress(Math.min(99, pct));
          }
        } catch (e) { }
      }
    }

    // Build a prefix byte-length array for the finalized `finalText` and
    // helper to map a byte offset to the nearest preceding character boundary.
    // This is more robust than walking `segments` because `finalText` may have
    // been trimmed/processed after `segments` were created.
    const finalLen = finalText ? finalText.length : 0;
    const prefixBytes = new Uint32Array(finalLen + 1);
    try {
      prefixBytes[0] = 0;
      // build prefixBytes cooperatively and show near-final progress
      await buildPrefixBytesAsync(finalText || '', prefixBytes, 95, 4);
    } catch (e) {
      dlog('[EPUBDbg] Failed to build prefixBytes: ' + (e && e.message));
    }
    function byteToCharIndexFromFinal(target) {
      if (!finalText || target <= 0) return 0;
      // binary search for largest i where prefixBytes[i] <= target
      let low = 0, high = finalLen;
      while (low < high) {
        const mid = Math.floor((low + high + 1) / 2);
        if (prefixBytes[mid] <= target) low = mid; else high = mid - 1;
      }
      return low;
    }
    if (tocEntriesAll && tocEntriesAll.length) {
      let visibleIndex = 1;
      const titleKeywordsRe = /^(目录|目錄|目次|contents?|table\s*of\s*contents?|sommaire|indice|índice)$/i;
      const entries = tocEntriesAll;
      const totalEntries = entries.length;
      const BATCH = 50;
      for (let start = 0; start < totalEntries; start += BATCH) {
        const end = Math.min(start + BATCH, totalEntries);
        for (let i = start; i < end; i++) {
          const e = entries[i];
          if (titleKeywordsRe.test((e.title || '').trim())) continue; // 过滤目录标题
          const resolved = canonicalPath(baseOpf, e.hrefPath, zip);
          if (standaloneTocFiles.has(resolved)) continue; // 整页目录（已裁剪）
          const chapterBase = chapterStartBytes.get(resolved) || 0;
          const key = e.fragment ? `${resolved}#${e.fragment}` : `${resolved}`;
          // raw position in original (after removalBytes subtraction)
          let rawBytePos = anchorAbsBytes.has(key) ? anchorAbsBytes.get(key) : chapterBase;
          rawBytePos = Math.max(0, rawBytePos - removalBytes);

          // Apply post-processing byte offset mapping if present
          let mappedBytePos = rawBytePos;
          if (byteOffsetMapper && typeof byteOffsetMapper === 'function') {
            try { mappedBytePos = byteOffsetMapper(rawBytePos); } catch (err) { dlog('[EPUBDbg] byteOffsetMapper threw: ' + (err && err.message)); }
          }
          if (mappedBytePos > cumulativeBytes) mappedBytePos = cumulativeBytes;
          // Ensure mappedBytePos is aligned to a UTF-8 character boundary by
          // converting to the nearest preceding character index and re-encoding
          try {
            const charIndex = byteToCharIndexFromFinal(mappedBytePos);
            mappedBytePos = prefixBytes[charIndex];
          } catch (err) { dlog('[EPUBDbg] align mappedBytePos failed: ' + (err && err.message)); }
        const pct = cumulativeBytes > 0 ? (mappedBytePos / cumulativeBytes * 100) : 0;

        // Verification: try to locate title text in finalText and compute its byte offset for comparison
        try {
          const titleText = (e.title || '').trim();
          let verifyByte = -1;
          if (titleText && finalText) {
            const lt = finalText.toLowerCase();
            const ti = titleText.toLowerCase();
            // collect all occurrences of the title in finalText
            const positions = [];
            let startPos = 0;
            while (true) {
              const p = lt.indexOf(ti, startPos);
              if (p < 0) break;
              positions.push(p);
              startPos = p + 1; // allow overlapping occurrences
            }
            if (positions.length) {
              // choose the candidate whose byte offset is nearest to mappedBytePos
              let best = null;
              for (const pos of positions) {
                const charIdx = Math.min(pos, finalLen);
                const b = prefixBytes[charIdx] || 0;
                const delta = Math.abs(mappedBytePos - b);
                if (best === null || delta < best.delta) best = { pos, b, delta };
              }
              if (best) verifyByte = best.b;
            }
          }
          // If title-based search failed, but we have an anchor absolute
          // position recorded, use that as a verify candidate (already
          // adjusted later by removalBytes when computing rawBytePos).
          if (verifyByte < 0 && anchorAbsBytes.has(key)) {
            try { verifyByte = Math.max(0, anchorAbsBytes.get(key) - removalBytes); } catch (e) { verifyByte = -1; }
          }
          // Log debug info for this idx entry
          const safeTitle = (e.title || '').replace(/[\r\n]+/g, ' ').slice(0, 120);
          dlog(`[EPUBDbg] IDX entry idx=${visibleIndex}, title='${safeTitle}', key='${key}', raw=${rawBytePos}, mapped=${mappedBytePos}, verify=${verifyByte}, cumulative=${cumulativeBytes}`);
          if (verifyByte >= 0) {
            const delta = mappedBytePos - verifyByte;
            if (Math.abs(delta) > 0) { dlog(`[EPUBDbg] IDX verify delta bytes=${delta} for title='${safeTitle}'`); }
            // Prefer a nearby verify match. Use a larger, conservative threshold
            // but select the nearest occurrence (implemented above). If the
            // nearest match is within this threshold, prefer it as the bytePos.
            const VERIFY_FALLBACK_THRESHOLD = 65536; // bytes (64KB)
            if (Math.abs(delta) > 0 && Math.abs(delta) <= VERIFY_FALLBACK_THRESHOLD) {
              dlog(`[EPUBDbg] IDX using verify fallback for title='${safeTitle}': mapped=${mappedBytePos} -> verify=${verifyByte}`);
              mappedBytePos = verifyByte;
            }
          }
        } catch (e) { /* ignore debug errors */ }

        idx.push({ index: visibleIndex++, title: e.title, bytePos: mappedBytePos, percent: pct });
        }
        // yield to UI after each batch
        await new Promise(r => setTimeout(r, 0));
      }
    }

    // 归一化：若首条 idx 的 bytePos 存在 1~6 的微小正偏移（可能是未被检测到的零宽字符/BOM 或前面剩余的极少空白），整体平移到 0
    if (idx.length) {
      const firstShift = idx[0].bytePos;
      if (firstShift > 0 && firstShift <= 6) {
        dlog('首条章节存在微小偏移 ' + firstShift + '，执行归一化为 0');
        for (const e of idx) {
          e.bytePos = Math.max(0, e.bytePos - firstShift);
          e.percent = cumulativeBytes > 0 ? (e.bytePos / cumulativeBytes * 100) : 0;
        }
      }
    }

    // 尝试提取封面图片（EPUB3: manifest item properties='cover-image'；EPUB2: metadata meta[name='cover'] 指向 id）
    let coverInfo = null;
    try {
      let coverId = null;
      try {
        const metaCover = pkgDoc.querySelector('metadata > meta[name="cover"]');
        if (metaCover) coverId = metaCover.getAttribute('content');
      } catch (e) { }
      // EPUB3 cover-image property
      if (!coverId) {
        for (const id in manifest) {
          try { if (manifest[id].props && manifest[id].props.split && manifest[id].props.split(/\s+/).includes('cover-image')) { coverId = id; break; } } catch (e) { }
        }
      }
      // fallback: manifest item with href or id contains 'cover' and is image
      if (!coverId) {
        for (const id in manifest) {
          const m = manifest[id];
          if (!m) continue;
          const media = (m.media || '').toLowerCase();
          if (media.startsWith('image/') && /cover|封面/i.test(m.href || id)) { coverId = id; break; }
        }
      }
      if (coverId && manifest[coverId]) {
        const chref = canonicalPath(baseOpf, manifest[coverId].href || '', zip);
        const f = zip.file(chref);
        if (f) {
          try {
            const arr = await f.async('uint8array');
            const mediaType = manifest[coverId].media || (chref.toLowerCase().endsWith('.png') ? 'image/png' : 'image/jpeg');
            const blob = new Blob([arr], { type: mediaType });
            const origName = (chref.split('/').pop() || 'cover');
            coverInfo = { name: origName, blob: blob, size: blob.size, media: mediaType };
          } catch (e) { /* ignore */ }
        }
      }
    } catch (e) { /* ignore */ }

    // 构建返回的meta信息，包含后处理统计
    const meta = {
      removalBytes,
      tocCount: tocEntriesAll.length,
      standaloneTocCount: standaloneTocFiles.size,
      stripNavCount: stripNavPages.size
    };

    // 添加后处理统计信息
    if (postProcessStats) {
      meta.postProcessStats = postProcessStats;
    }

    return { text: finalText, idxEntries: idx, meta, cover: coverInfo };
  }

  async function processQueue() {
    if (running) return;
    if (files.length === 0) { alert('请选择 EPUB 文件'); return; }
    running = true;
    abortRequested = false;
    updateButtons();
    convertInfo.innerHTML = '';
    const total = files.length;
    let processed = 0;
    const summary = [];
    log('开始批量转换 ' + total + ' 个文件');
    for (let idx = 0; idx < files.length; idx++) {
      const item = files[idx];
      if (abortRequested) {
        setStatus(item, '已取消', 'cancelled');
        setItemProgress(item, 0);
        continue;
      }
      setStatus(item, '转换中...', 'running');
      setItemProgress(item, 0);
      convertInfo.innerHTML = `<div class="convert-summary convert-summary-small">正在转换：<strong>${escapeHtml(item.file.name)}</strong> (${idx + 1}/${total})</div>`;
      try {
        const ab = await item.file.arrayBuffer();
        const res = await epubToText(ab, (frac) => {
          setItemProgress(item, frac);
          if (total > 0) setProgress(((processed + frac) / total) * 100);
        });
        if (!res || !res.text) throw new Error('未生成文本内容');
        const base = item.file.name.replace(/\.epub$/i, '').replace(/\.[^.]+$/, '');
        const outName = base + '.txt';
        let idxCount = 0;
        // 生成文件对象（不自动触发下载），在页面显示下载链接/打包按钮
        const outputs = [];
        const txtBlob = new Blob([res.text], { type: 'text/plain;charset=utf-8' });
        outputs.push({ name: outName, blob: txtBlob, size: txtBlob.size, type: 'text' });
        // 如果提取到了封面图片，则加入输出列表（文件名以 base 前缀）
        if (res && res.cover && res.cover.blob) {
          try {
            const orig = res.cover.name || 'cover';
            const dotIdx = orig.lastIndexOf('.');
            const ext = dotIdx >= 0 ? orig.slice(dotIdx) : (res.cover.media && res.cover.media.indexOf('png')>=0?'.png':'.jpg');
            const coverName = base + '_cover' + ext;
            outputs.push({ name: coverName, blob: res.cover.blob, size: res.cover.blob.size, type: 'image' });
          } catch (e) { /* ignore cover failures */ }
        }
        if (optExtractToc && optExtractToc.checked && res.idxEntries && res.idxEntries.length) {
          const idxLines = res.idxEntries.map((e) => `#${e.index}#, #${e.title.replace(/[#\r\n]/g, '').trim()}#, #${e.bytePos}#, #${e.percent.toFixed(2)}#,`).join('\n');
          const idxBlob = new Blob([idxLines], { type: 'text/plain;charset=utf-8' });
          outputs.push({ name: base + '.idx', blob: idxBlob, size: idxBlob.size, type: 'idx' });
          idxCount = res.idxEntries.length;
        }
        // store generated group and render download links
        generated.push({ base, outputs });
        renderDownloadArea();
        log('已生成: ' + outName + (idxCount > 0 ? (' & ' + base + '.idx') : ''));
        const extraParts = [];
        if (res.meta) {
          const trimmed = res.meta.removalBytes || 0;
          if (trimmed > 0) extraParts.push(`裁剪目录 ${fmtSize(trimmed)}`);
          extraParts.push(`TOC:${res.meta.tocCount} 整页:${res.meta.standaloneTocCount} 混合:${res.meta.stripNavCount}`);

          // 添加后处理统计信息
          if (res.meta.postProcessStats && res.meta.postProcessStats.RemoveBlankLines) {
            const stats = res.meta.postProcessStats.RemoveBlankLines;
            extraParts.push(`空行:${stats.linesRemoved}行/${fmtSize(stats.bytesRemoved)}`);
          }
        }
        if (idxCount > 0) extraParts.push(`导出 idx ${idxCount} 条`);
        const extra = extraParts.join('；');
        setStatus(item, '已完成', 'success');
        setItemProgress(item, 1);
        // 使用 txtBlob 替代之前的临时变量 blob（已改为不自动下载）
        log('已生成: ' + outName + ' (大小: ' + fmtSize(txtBlob.size) + ')');
        summary.push({ success: true, name: item.file.name, outName, size: txtBlob.size, extra, idxCount });
      } catch (e) {
        const msg = (e && e.message) ? e.message : String(e);
        if (msg === '已取消') {
          setStatus(item, '已取消', 'cancelled');
          setItemProgress(item, 0);
          summary.push({ success: false, name: item.file.name, error: '已取消', cancelled: true });
          log('用户取消了转换: ' + item.file.name);
          abortRequested = true;
          break;
        } else {
          setStatus(item, '失败', 'error');
          setItemProgress(item, 0);
          summary.push({ success: false, name: item.file.name, error: msg });
          log('转换失败: ' + item.file.name + ' -> ' + msg);
        }
      }
      processed += 1;
      if (total > 0) setProgress((processed / total) * 100);
    }
    if (abortRequested) {
      files.forEach(item => {
        if (item.statusType === 'pending') {
          setStatus(item, '已取消', 'cancelled');
          setItemProgress(item, 0);
          summary.push({ success: false, name: item.file.name, error: '已取消', cancelled: true });
        }
      });
      log('批量转换已取消');
    } else {
      log('批量转换完成');
    }
    running = false;
    abortRequested = false;
    updateButtons();
    renderSummary(summary);
    if (!summary.some(it => it.success)) {
      setTimeout(() => setProgress(0), 300);
    }
  }

  function renderSummary(summary) {
    if (!convertInfo) return;
    if (!summary || !summary.length) {
      convertInfo.innerHTML = '<div class="muted">暂无转换结果</div>';
      return;
    }
    const success = summary.filter(it => it.success);
    const cancelled = summary.filter(it => !it.success && it.cancelled);
    const failed = summary.filter(it => !it.success && !it.cancelled);
    const titleParts = [];
    titleParts.push(`${success.length} 成功`);
    if (failed.length) titleParts.push(`${failed.length} 失败`);
    if (cancelled.length) titleParts.push(`${cancelled.length} 取消`);
    let html = `<div class="convert-summary"><div class="convert-summary-title">转换汇总 (${titleParts.join(' / ')})</div>`;
    if (success.length) {
      const totalSize = success.reduce((acc, it) => acc + (it.size || 0), 0);
      html += '<table class="convert-summary-table" role="table"><thead><tr><th class="col-index">#</th><th class="col-name">源文件</th><th class="col-size">大小</th><th class="col-remark">备注</th></tr></thead><tbody>';
      success.forEach((item, idx) => {
        html += `<tr><td class="col-index">${idx + 1}</td><td class="col-name"><span class="truncate">${escapeHtml(item.name)}</span></td><td class="col-size">${fmtSize(item.size || 0)}</td><td class="col-remark remark-cell">${escapeHtml(item.extra || '')}</td></tr>`;
      });
      html += `</tbody></table><div class="convert-summary-footer">成功输出共 ${fmtSize(totalSize)}</div>`;
    }
    if (failed.length) {
      html += '<details class="convert-summary-details" open><summary>失败列表</summary><ul>';
      failed.forEach(item => {
        html += `<li>${escapeHtml(item.name)} — ${escapeHtml(item.error || '未知错误')}</li>`;
      });
      html += '</ul></details>';
    }
    if (cancelled.length) {
      html += '<details class="convert-summary-details" open><summary>取消的文件</summary><ul>';
      cancelled.forEach(item => {
        html += `<li>${escapeHtml(item.name)}</li>`;
      });
      html += '</ul></details>';
    }
    html += '</div>';
    convertInfo.innerHTML = html;
  }

  generateBtn.onclick = () => { processQueue(); };
  cancelBtn.onclick = () => { if (!running) return; abortRequested = true; log('用户请求取消'); cancelBtn.disabled = true; };

  function downloadBlob(blob, filename) {
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a'); a.href = url; a.download = filename; a.classList.add('download-link'); document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 5000);
  }

  // expose helpers for debugging / integration
  window.__epubConvertor = {
    addFiles: addFilesFromList,
    clear: clearQueue,
    setFile(file) { clearQueue(); if (file) addFilesFromList([file]); }
  };

  rebuildFileList();
  updateButtons();
  log('EPUB 转换器 已加载');
});
