(function(){
  // 简单的客户端片段加载器
  async function includeOnce(el, url) {
    try {
      const res = await fetch(url, {cache: 'no-store'});
      if (!res.ok) throw new Error('HTTP '+res.status);
      const text = await res.text();
      // Insert HTML
      el.innerHTML = text;

      // Execute any scripts present in the included fragment (inline or external)
      try{
        executeScripts(el);
      }catch(e){ console.error('executeScripts failed', e); }

      // 标记当前页面对应的 tab 为 active
      try {
        markActiveTab(el);
      } catch (e) {
        // ignore
      }
    } catch (e) {
      console.error('include partial failed', url, e);
      el.innerHTML = '<!-- failed to load ' + url + ' -->';
    }
  }

  // Find <script> tags inside container and execute them in document context.
  // For external scripts, we create a new script element to ensure browser loads it.
  function executeScripts(container){
    const scripts = Array.from(container.querySelectorAll('script'));
    scripts.forEach(s => {
      const newScript = document.createElement('script');
      // copy attributes
      for (let i = 0; i < s.attributes.length; i++){
        const attr = s.attributes[i];
        newScript.setAttribute(attr.name, attr.value);
      }
      if (s.src){
        // use full url resolution relative to the include file location if possible
        // let browser resolve relative src when appended to head
        newScript.src = s.src;
        newScript.async = false; // preserve execution order
        document.head.appendChild(newScript);
      } else {
        newScript.textContent = s.textContent;
        document.head.appendChild(newScript);
      }
      // remove original script tag to avoid duplicate execution if container is re-inserted
      s.parentNode && s.parentNode.removeChild(s);
    });
  }

  // 根据当前页面文件名选择对应的 nav tab 并添加 active 类
  function markActiveTab(container) {
    if (!container) return;
    const tabs = container.querySelectorAll('.tabs a');
    if (!tabs || tabs.length === 0) return;

    // 获取当前地址的文件名
    let path = window.location.pathname || window.location.href || '';
    let filename = path.split('/').pop() || '';
    // 如果为空（例如访问根），尝试从 hash 或 search 中解析包含的文件名
    if (!filename) {
      const h = window.location.hash || '';
      if (h) filename = h.replace('#', '').split('/').pop();
    }
    // 取默认页面名（main.html）作为回退
    if (!filename) filename = 'main.html';

    tabs.forEach(a => {
      // 只比较链接的文件名部分
      const href = a.getAttribute('href') || '';
      const hrefName = href.split('/').pop();
      if (hrefName && hrefName === filename) {
        a.classList.add('active');
        a.classList.remove('text-dark');
      } else {
        a.classList.remove('active');
      }
    });
  }

  document.addEventListener('DOMContentLoaded', function(){
    document.querySelectorAll('[data-include]').forEach(el=>{
      const url = el.getAttribute('data-include');
      // 相对于当前页面的路径，保持简单
      includeOnce(el, url);
    });
  });
})();
