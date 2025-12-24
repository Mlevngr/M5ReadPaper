// 获取并显示版本号
if (typeof chrome !== 'undefined' && chrome.runtime) {
    const manifest = chrome.runtime.getManifest();
    document.getElementById('versionBadge').textContent = 'v' + manifest.version;
}

// "稍后查看" - 关闭当前标签页
document.getElementById('btnLater').addEventListener('click', () => {
    if (typeof chrome !== 'undefined' && chrome.tabs) {
        chrome.tabs.getCurrent((tab) => {
            if (tab && tab.id) {
                chrome.tabs.remove(tab.id);
            } else {
                window.close();
            }
        });
    } else {
        window.close();
    }
});

// "开始使用" - 跳转到欢迎页
document.getElementById('btnStart').addEventListener('click', () => {
    if (typeof chrome !== 'undefined' && chrome.tabs) {
        // 在当前标签页打开欢迎页
        window.location.href = 'welcome.html';
    } else {
        window.location.href = 'welcome.html';
    }
});

// ESC 键关闭
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        document.getElementById('btnLater').click();
    }
});
