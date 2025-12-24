(function(){
    const STORAGE_KEY = 'readpaper_last_version';
    const hasChrome = typeof chrome !== 'undefined';
    const hasRuntime = hasChrome && !!chrome.runtime && !!chrome.runtime.getURL;
    const hasStorage = hasChrome && chrome.storage && chrome.storage.local;

    function getUpdatePageUrl(){
        return hasRuntime ? chrome.runtime.getURL('pages/version-update.html') : 'version-update.html';
    }

    function withChromeStorage(action){
        return new Promise((resolve, reject) => {
            try {
                action((value) => {
                    const err = hasChrome && chrome.runtime && chrome.runtime.lastError;
                    if (err) {
                        reject(err);
                    } else {
                        resolve(value);
                    }
                });
            } catch (error) {
                reject(error);
            }
        });
    }

    function sendRuntimeMessage(payload){
        return new Promise((resolve, reject) => {
            if (!(hasRuntime && chrome.runtime && chrome.runtime.sendMessage)) {
                reject(new Error('runtime messaging unavailable'));
                return;
            }
            try {
                chrome.runtime.sendMessage(payload, (response) => {
                    const err = chrome.runtime.lastError;
                    if (err) {
                        reject(err);
                    } else {
                        resolve(response);
                    }
                });
            } catch (error) {
                reject(error);
            }
        });
    }

    async function fetchCurrentVersion(){
        if (hasRuntime && chrome.runtime.getManifest) {
            return chrome.runtime.getManifest().version;
        }
        try {
            const response = await fetch('../manifest.json');
            if (response.ok) {
                const manifest = await response.json();
                return manifest.version || '-';
            }
        } catch (error) {
            console.warn('[version-test] 无法读取 manifest.json:', error);
        }
        return '-';
    }

    async function getStoredVersion(){
        if (hasStorage) {
            const result = await withChromeStorage((done) => chrome.storage.local.get(STORAGE_KEY, done));
            return result && result[STORAGE_KEY] ? result[STORAGE_KEY] : '';
        }
        return localStorage.getItem(STORAGE_KEY) || '';
    }

    async function setStoredVersion(version){
        if (hasStorage) {
            await withChromeStorage((done) => chrome.storage.local.set({ [STORAGE_KEY]: version }, done));
        } else {
            localStorage.setItem(STORAGE_KEY, version);
        }
    }

    async function clearStoredVersion(){
        if (hasStorage) {
            await withChromeStorage((done) => chrome.storage.local.remove(STORAGE_KEY, done));
        } else {
            localStorage.removeItem(STORAGE_KEY);
        }
    }

    function compareVersions(current, stored){
        if (!current || current === '-') return false;
        if (!stored) return true;
        return current !== stored;
    }

    function setStatusMessage(text, type){
        const el = document.getElementById('statusMessage');
        if (!el) return;
        el.textContent = text || '\u00A0';
        el.className = type === 'error' ? 'text-error' : type === 'success' ? 'text-primary' : type === 'info' ? 'text-grey' : 'muted';
    }

    async function refreshStatus(){
        const current = await fetchCurrentVersion();
        const stored = await getStoredVersion();
        const needsUpdate = compareVersions(current, stored);

        const currentEl = document.getElementById('currentVersion');
        const storedEl = document.getElementById('storedVersion');
        const needsEl = document.getElementById('needsUpdate');
        if (currentEl) currentEl.textContent = current;
        if (storedEl) storedEl.textContent = stored || '未记录';
        if (needsEl) {
            needsEl.textContent = needsUpdate ? '是' : '否';
            needsEl.className = needsUpdate ? 'text-primary' : 'text-grey';
        }
    }

    async function handleForceCheck(){
        if (!(hasRuntime && chrome.runtime && chrome.runtime.sendMessage)) {
            setStatusMessage('当前环境不支持后台脚本，直接打开版本页以演示效果。', 'error');
            openUpdatePage(false);
            return;
        }
        setStatusMessage('正在请求后台检测…', 'info');
        try {
            const response = await sendRuntimeMessage({ type: 'forceVersionCheck' });
            if (response && response.ok) {
                setStatusMessage('后台检测已触发，请查看是否自动打开新标签页。', 'success');
                setTimeout(() => { refreshStatus(); }, 1200);
            } else {
                setStatusMessage('后台检测调用失败：' + (response && response.error ? response.error : '未知错误'), 'error');
            }
        } catch (error) {
            setStatusMessage('后台检测调用异常：' + (error && error.message ? error.message : String(error)), 'error');
        }
    }

    function openUpdatePage(newTab){
        const url = getUpdatePageUrl();
        if (newTab) {
            if (hasChrome && chrome.tabs && chrome.tabs.create) {
                chrome.tabs.create({ url });
            } else {
                window.open(url, '_blank');
            }
        } else {
            window.location.href = url;
        }
    }

    function bindControls(){
        const openTab = document.getElementById('btnOpenTab');
        const openCurrent = document.getElementById('btnOpenCurrent');
        const forceCheck = document.getElementById('btnForceCheck');
        const resetBtn = document.getElementById('btnResetStorage');
        const markBtn = document.getElementById('btnMarkCurrent');
        const simulateBtn = document.getElementById('btnSimulateOld');

        if (openTab) openTab.addEventListener('click', () => openUpdatePage(true));
        if (openCurrent) openCurrent.addEventListener('click', () => openUpdatePage(false));
        if (forceCheck) forceCheck.addEventListener('click', (e) => { e.preventDefault(); handleForceCheck(); });

        if (resetBtn) resetBtn.addEventListener('click', async () => {
            await clearStoredVersion();
            setStatusMessage('已清除存储的版本号。', 'success');
            await refreshStatus();
        });

        if (markBtn) markBtn.addEventListener('click', async () => {
            const current = await fetchCurrentVersion();
            if (!current || current === '-') {
                setStatusMessage('无法获取当前版本，请确认 manifest.json 可访问。', 'error');
                return;
            }
            await setStoredVersion(current);
            setStatusMessage('已将当前版本标记为已读。', 'success');
            await refreshStatus();
        });

        if (simulateBtn) simulateBtn.addEventListener('click', async () => {
            const current = await fetchCurrentVersion();
            let simulated = '0.0.1';
            if (current && current !== '-' && current !== '0.0.1') {
                simulated = '0.0.1';
            }
            await setStoredVersion(simulated);
            setStatusMessage('已写入旧版本号 ' + simulated + '，下次检测会被视为更新。', 'success');
            await refreshStatus();
        });
    }

    document.addEventListener('DOMContentLoaded', () => {
        bindControls();
        refreshStatus().catch((error) => {
            console.error('[version-test] 初始化失败:', error);
            setStatusMessage('初始化失败：' + (error.message || String(error)), 'error');
        });
    });
})();
