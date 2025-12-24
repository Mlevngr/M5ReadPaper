// Basic client-side image preprocess: load -> draw -> apply threshold/dither -> output
(function(){
    const fileInput = document.getElementById('fileInput');
    const srcCanvas = document.getElementById('srcCanvas');
    const outCanvas = document.getElementById('outCanvas');
    const ctxSrc = srcCanvas.getContext('2d');
    const ctxOut = outCanvas.getContext('2d');
    // offscreen original buffer: keep original resized pixels here so each reprocess uses unmodified source
    const origCanvas = document.createElement('canvas');
    const ctxOrig = origCanvas.getContext('2d');

    const scaleRange = document.getElementById('scaleRange');
    const scaleVal = document.getElementById('scaleVal');
    const thresholdRange = document.getElementById('thresholdRange');
    const thresholdVal = document.getElementById('thresholdVal');
    const ditherMode = document.getElementById('ditherMode');
    const grayscaleMode = document.getElementById('grayscaleMode');
    const grayLevels = document.getElementById('grayLevels');
    const backgroundFill = document.getElementById('backgroundFill');
    const magicTolerance = document.getElementById('magicTolerance');
    const magicToleranceVal = document.getElementById('magicToleranceVal');
    const magicModeBtn = document.getElementById('magicModeBtn');
    const magicUndoBtn = document.getElementById('magicUndoBtn');
    const outWidth = document.getElementById('outWidth');
    const outHeight = document.getElementById('outHeight');
    const resetSize = document.getElementById('resetSize');
    const applyBtn = document.getElementById('applyBtn');
    const downloadBtn = document.getElementById('downloadBtn');

    let img = new Image();
    // Pan state: offsetX/Y represent the position of processed image within the target output frame
    // (user drags the image to position it within the output box)
    let panOffsetX = 0, panOffsetY = 0;
    let isDragging = false;
    let dragStartX = 0, dragStartY = 0;
    let currentScale = 1.0; // Current display scale (independent of scaleRange for initial sizing)
    let isMagicMode = false;

    let transparentMask = null;
    let maskWidth = 0;
    let maskHeight = 0;
    let maskHistory = [];
    const MAGIC_HISTORY_LIMIT = 20;

    const checkerPatternCanvas = document.createElement('canvas');
    checkerPatternCanvas.width = checkerPatternCanvas.height = 16;
    const checkerCtx = checkerPatternCanvas.getContext('2d');
    checkerCtx.fillStyle = '#d0d0d0';
    checkerCtx.fillRect(0,0,16,16);
    checkerCtx.fillStyle = '#f0f0f0';
    checkerCtx.fillRect(0,0,8,8);
    checkerCtx.fillRect(8,8,8,8);

    function updateScaleLabel(){ scaleVal.textContent = scaleRange.value + '%'; }
    function updateThresholdLabel(){ thresholdVal.textContent = thresholdRange.value; }
    function updateMagicToleranceLabel(){ magicToleranceVal.textContent = magicTolerance.value; }

    function updateUndoState(){
        if(magicUndoBtn){
            magicUndoBtn.disabled = maskHistory.length === 0;
        }
    }

    function clearMaskHistory(){
        maskHistory.length = 0;
        updateUndoState();
    }

    function pushMaskHistory(snapshot){
        maskHistory.push(snapshot);
        if(maskHistory.length > MAGIC_HISTORY_LIMIT){
            maskHistory.shift();
        }
        updateUndoState();
    }

    function resetMask(w, h){
        maskWidth = w;
        maskHeight = h;
        transparentMask = new Uint8Array(w * h);
        clearMaskHistory();
    }

    function ensureMask(w, h){
        if(!transparentMask || maskWidth !== w || maskHeight !== h){
            resetMask(w, h);
        }
    }

    function fillPreviewBackground(ctx, ow, oh, fillColor){
        ctx.clearRect(0, 0, ow, oh);
        if(fillColor === 'transparent'){
            const pattern = ctx.createPattern(checkerPatternCanvas, 'repeat');
            if(pattern){
                ctx.fillStyle = pattern;
                ctx.fillRect(0, 0, ow, oh);
            }
        } else {
            ctx.fillStyle = fillColor;
            ctx.fillRect(0, 0, ow, oh);
        }
    }

    function setCanvasCursor(){
        if(isMagicMode){
            srcCanvas.style.cursor = 'crosshair';
        } else if(isDragging){
            srcCanvas.style.cursor = 'grabbing';
        } else if(img && img.width > 0){
            srcCanvas.style.cursor = 'grab';
        } else {
            srcCanvas.style.cursor = 'default';
        }
    }

    function toggleMagicMode(){
        isMagicMode = !isMagicMode;
        isDragging = false;
        magicModeBtn.classList.toggle('btn-primary', isMagicMode);
        setCanvasCursor();
    }

    function getCanvasCoords(e){
        const rect = srcCanvas.getBoundingClientRect();
        const scaleX = srcCanvas.width / rect.width;
        const scaleY = srcCanvas.height / rect.height;
        const x = (e.clientX - rect.left) * scaleX;
        const y = (e.clientY - rect.top) * scaleY;
        return { x, y };
    }

    scaleRange.addEventListener('input', ()=> { updateScaleLabel(); drawSrc(); });
    thresholdRange.addEventListener('input', ()=> { updateThresholdLabel(); applyProcess(); });
    magicTolerance.addEventListener('input', () => { updateMagicToleranceLabel(); });
    magicModeBtn.addEventListener('click', (e)=>{ e.preventDefault(); if(outCanvas.width){ toggleMagicMode(); }});
    magicUndoBtn.addEventListener('click', (e)=>{ e.preventDefault(); undoMagicSelection(); });

    fileInput.addEventListener('change', (ev) => {
        const f = ev.target.files[0];
        if(!f) return;
        const url = URL.createObjectURL(f);
        img.onload = () => {
            URL.revokeObjectURL(url);
            // Reset state on new image
            panOffsetX = 0; panOffsetY = 0;
            currentScale = 1.0;
            scaleRange.value = 100;
            updateScaleLabel();
            updateMagicToleranceLabel();
            drawSrc();
        };
        img.src = url;
    });

    function drawSrc(){
        if(!img || img.width===0) return;

        // Determine BASE scaled image size (fit inside target output, preserving aspect ratio)
        // This is the "100%" reference size
        const ow = parseInt(outWidth.value) || 540;
        const oh = parseInt(outHeight.value) || 960;
        const srcW = img.width, srcH = img.height;
        
        // Calculate base size that fits inside output dimensions
        const sx = ow / srcW;
        const sy = oh / srcH;
        const baseScale = Math.min(sx, sy);
        const baseW = Math.max(1, Math.round(srcW * baseScale));
        const baseH = Math.max(1, Math.round(srcH * baseScale));

        // Apply user's scale percentage on top of base size
        currentScale = scaleRange.value / 100;
        const w = Math.max(1, Math.round(baseW * currentScale));
        const h = Math.max(1, Math.round(baseH * currentScale));

        // draw the resized original into the offscreen buffer
        origCanvas.width = w; origCanvas.height = h;
        ctxOrig.clearRect(0,0,w,h);
        ctxOrig.drawImage(img, 0,0,w,h);
        
        // Set outCanvas to match processed size for internal use
        outCanvas.width = w; outCanvas.height = h;

        resetMask(w, h);
        magicModeBtn.classList.remove('btn-primary');
        isMagicMode = false;
        setCanvasCursor();
        
        // auto apply preview (will draw into srcCanvas at target output size)
        applyProcess();
    }

    function applyProcess(){
        if(!img || img.width===0) return;
        const w = origCanvas.width, h = origCanvas.height;
        const src = ctxOrig.getImageData(0,0,w,h);
        const out = ctxOut.createImageData(w,h);
        const threshold = parseInt(thresholdRange.value);
        const gray = grayscaleMode.value;
        const levels = parseInt(grayLevels.value) || 16;
        const dither = ditherMode.value;

        // copy + process
        if(dither === 'floyd'){
            // simple Floyd–Steinberg dithering on luminance
            const lum = new Float32Array(w*h);
            for(let y=0;y<h;y++){
                for(let x=0;x<w;x++){
                    const i = (y*w+x)*4;
                    const r = src.data[i], g = src.data[i+1], b = src.data[i+2];
                    let v = 0.2126*r + 0.7152*g + 0.0722*b;
                    if(gray === 'none') v = (r+g+b)/3;
                    lum[y*w+x] = v;
                }
            }
            for(let y=0;y<h;y++){
                for(let x=0;x<w;x++){
                    const idx = y*w+x;
                    const oldv = lum[idx];
                    let newv;
                    if(levels === 2){
                        newv = oldv < threshold ? 0 : 255;
                    } else {
                        const q = Math.round((levels - 1) * (oldv / 255));
                        newv = (levels > 1) ? (q * (255 / (levels - 1))) : oldv;
                    }
                    const err = oldv - newv;
                    lum[idx] = newv;
                    if(x+1 < w) lum[idx+1] += err * 7/16;
                    if(x-1 >=0 && y+1 < h) lum[idx + w -1] += err * 3/16;
                    if(y+1 < h) lum[idx + w] += err * 5/16;
                    if(x+1 < w && y+1 < h) lum[idx + w +1] += err * 1/16;
                }
            }
            for(let y=0;y<h;y++){
                for(let x=0;x<w;x++){
                    const i = (y*w+x)*4;
                    const v = (levels === 2) ? (lum[y*w+x] < 128 ? 0 : 255) : Math.max(0, Math.min(255, Math.round(lum[y*w+x])));
                    out.data[i]=out.data[i+1]=out.data[i+2]=v;
                    out.data[i+3]=255;
                }
            }
        } else {
            for(let i=0;i<src.data.length;i+=4){
                const r = src.data[i], g = src.data[i+1], b = src.data[i+2];
                let lumv = 0.2126*r + 0.7152*g + 0.0722*b;
                if(gray === 'none') lumv = (r+g+b)/3;
                let v;
                if(levels === 2){
                    v = lumv < threshold ? 0 : 255;
                } else {
                    const q = Math.round((levels - 1) * (lumv / 255));
                    v = (levels > 1) ? (q * (255 / (levels - 1))) : lumv;
                }
                out.data[i]=out.data[i+1]=out.data[i+2]=Math.max(0, Math.min(255, Math.round(v)));
                out.data[i+3]=255;
            }
        }

        ensureMask(w, h);
        if(transparentMask){
            for(let i=0;i<w*h;i++){
                if(transparentMask[i]){
                    out.data[i*4 + 3] = 0;
                }
            }
        }

        // Store processed image in outCanvas
        ctxOut.putImageData(out,0,0);
        
        // Now render the preview: show target output frame with processed image positioned by panOffset
        const ow = parseInt(outWidth.value) || 540;
        const oh = parseInt(outHeight.value) || 960;
        
        // Set srcCanvas to target output size
        srcCanvas.width = ow;
        srcCanvas.height = oh;
        
        const fillColor = backgroundFill.value || '#ffffff';

        // Fill with selected background color
        fillPreviewBackground(ctxSrc, ow, oh, fillColor);
        
        // Draw processed image at panOffset position
        // Clamp panOffset so image stays reasonably within bounds (allow partial visibility)
        const maxOffsetX = ow;
        const maxOffsetY = oh;
        const minOffsetX = -w;
        const minOffsetY = -h;
        panOffsetX = Math.max(minOffsetX, Math.min(maxOffsetX, panOffsetX));
        panOffsetY = Math.max(minOffsetY, Math.min(maxOffsetY, panOffsetY));
        
        ctxSrc.drawImage(outCanvas, 0, 0, w, h, panOffsetX, panOffsetY, w, h);
        
        // Draw a subtle border around the output frame to indicate boundaries
        ctxSrc.strokeStyle = '#ccc';
        ctxSrc.lineWidth = 1;
        ctxSrc.strokeRect(0, 0, ow, oh);
    }

    function applyMagicWandAt(canvasX, canvasY){
        const w = outCanvas.width;
        const h = outCanvas.height;
        if(w === 0 || h === 0) return;

        const imgX = Math.floor(canvasX - panOffsetX);
        const imgY = Math.floor(canvasY - panOffsetY);
        if(imgX < 0 || imgY < 0 || imgX >= w || imgY >= h) return;

        ensureMask(w, h);
        const prevMask = transparentMask ? transparentMask.slice() : null;
        const toleranceVal = parseInt(magicTolerance.value, 10);
        const tolerance = Number.isFinite(toleranceVal) ? toleranceVal : 32;
        const tolSq = tolerance * tolerance;

        const imageData = ctxOut.getImageData(0,0,w,h);
        const data = imageData.data;
        const targetIdx = (imgY * w + imgX) * 4;
        const targetR = data[targetIdx];
        const targetG = data[targetIdx + 1];
        const targetB = data[targetIdx + 2];

        const visited = new Uint8Array(w * h);
        const stack = [[imgX, imgY]];
        let changed = false;

        while(stack.length){
            const [x, y] = stack.pop();
            if(x < 0 || y < 0 || x >= w || y >= h) continue;
            const idx = y * w + x;
            if(visited[idx]) continue;
            visited[idx] = 1;
            const dataIdx = idx * 4;
            const dr = data[dataIdx] - targetR;
            const dg = data[dataIdx + 1] - targetG;
            const db = data[dataIdx + 2] - targetB;
            const distSq = dr*dr + dg*dg + db*db;
            if(distSq > tolSq) continue;
            if(!transparentMask[idx]){
                transparentMask[idx] = 1;
                changed = true;
            }
            stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
        }

        if(changed){
            if(prevMask) pushMaskHistory(prevMask);
            applyProcess();
        }
    }

    function undoMagicSelection(){
        if(maskHistory.length === 0) return;
        const previous = maskHistory.pop();
        transparentMask = previous;
        maskWidth = outCanvas.width;
        maskHeight = outCanvas.height;
        updateUndoState();
        applyProcess();
    }

    // Real-time preview: update when controls change
    ditherMode.addEventListener('change', applyProcess);
    grayscaleMode.addEventListener('change', applyProcess);
    grayLevels.addEventListener('change', applyProcess);
    backgroundFill.addEventListener('change', applyProcess);
    outWidth.addEventListener('input', drawSrc);
    outHeight.addEventListener('input', drawSrc);
    resetSize.addEventListener('click', (e)=>{ e.preventDefault(); outWidth.value='540'; outHeight.value='960'; panOffsetX=0; panOffsetY=0; drawSrc(); });

    if(applyBtn){
        applyBtn.addEventListener('click', (e)=>{ applyProcess(); });
    }

    // Mouse drag to position image within target output frame
    srcCanvas.addEventListener('mousedown', (e)=>{
        if(!img || img.width===0) return;
        if(isMagicMode){
            e.preventDefault();
            const { x, y } = getCanvasCoords(e);
            applyMagicWandAt(x, y);
            return;
        }
        isDragging = true;
        const rect = srcCanvas.getBoundingClientRect();
        dragStartX = e.clientX - rect.left;
        dragStartY = e.clientY - rect.top;
        setCanvasCursor();
    });
    
    srcCanvas.addEventListener('mousemove', (e)=>{
        if(isMagicMode || !isDragging) return;
        const rect = srcCanvas.getBoundingClientRect();
        const mx = e.clientX - rect.left;
        const my = e.clientY - rect.top;
        // Calculate delta in canvas coordinates
        const scaleX = srcCanvas.width / rect.width;
        const scaleY = srcCanvas.height / rect.height;
        const dx = (mx - dragStartX) * scaleX;
        const dy = (my - dragStartY) * scaleY;
        panOffsetX += dx;
        panOffsetY += dy;
        dragStartX = mx;
        dragStartY = my;
        applyProcess();
    });
    
    srcCanvas.addEventListener('mouseup', ()=>{ 
        isDragging = false; 
        setCanvasCursor();
    });
    
    srcCanvas.addEventListener('mouseleave', ()=>{ 
        isDragging = false;
        setCanvasCursor();
    });
    
    srcCanvas.addEventListener('mouseenter', ()=>{
        setCanvasCursor();
    });
    // Touch events for mobile: map single-finger drag to pan
    srcCanvas.addEventListener('touchstart', (e)=>{
        if(!img || img.width===0) return;
        // only handle single-touch
        if(e.touches.length !== 1) return;
        // prevent page scrolling while interacting with canvas
        e.preventDefault();
        const t = e.touches[0];
        if(isMagicMode){
            const { x, y } = getCanvasCoords(t);
            applyMagicWandAt(x, y);
            return;
        }
        isDragging = true;
        const rect = srcCanvas.getBoundingClientRect();
        dragStartX = t.clientX - rect.left;
        dragStartY = t.clientY - rect.top;
        setCanvasCursor();
    }, { passive: false });

    srcCanvas.addEventListener('touchmove', (e)=>{
        if(!img || img.width===0) return;
        if(isMagicMode || !isDragging) return;
        if(e.touches.length !== 1) return;
        e.preventDefault();
        const t = e.touches[0];
        const rect = srcCanvas.getBoundingClientRect();
        const mx = t.clientX - rect.left;
        const my = t.clientY - rect.top;
        const scaleX = srcCanvas.width / rect.width;
        const scaleY = srcCanvas.height / rect.height;
        const dx = (mx - dragStartX) * scaleX;
        const dy = (my - dragStartY) * scaleY;
        panOffsetX += dx;
        panOffsetY += dy;
        dragStartX = mx;
        dragStartY = my;
        applyProcess();
    }, { passive: false });

    srcCanvas.addEventListener('touchend', (e)=>{
        // stop dragging on touch end
        isDragging = false;
        setCanvasCursor();
    });

    srcCanvas.addEventListener('touchcancel', (e)=>{
        isDragging = false;
        setCanvasCursor();
    });
    setCanvasCursor();

    downloadBtn.addEventListener('click', ()=>{
        if(outCanvas.width===0) return;
        const ow = parseInt(outWidth.value) || 540;
        const oh = parseInt(outHeight.value) || 960;
        
        // Create final output canvas at exact target size
        const finalCanvas = document.createElement('canvas');
        finalCanvas.width = ow;
        finalCanvas.height = oh;
        const fctx = finalCanvas.getContext('2d');
        
        // Fill with selected background color to match preview (allow transparent)
        const exportFill = backgroundFill.value || '#ffffff';
        if(exportFill === 'transparent'){
            fctx.clearRect(0, 0, ow, oh);
        } else {
            fctx.fillStyle = exportFill;
            fctx.fillRect(0, 0, ow, oh);
        }
        
        // Draw processed image at the same position as shown in preview
        fctx.drawImage(outCanvas, 0, 0, outCanvas.width, outCanvas.height, 
                       panOffsetX, panOffsetY, outCanvas.width, outCanvas.height);
        
        const a = document.createElement('a');
        a.href = finalCanvas.toDataURL('image/png');
        a.download = 'pichandle_result.png';
        document.body.appendChild(a);
        a.click();
        a.remove();
    });

    // initial labels
    updateScaleLabel(); updateThresholdLabel(); updateMagicToleranceLabel(); updateUndoState();
    
    // Sync right panel card height with left panel card height on desktop
    function syncCardHeights() {
        if (window.innerWidth >= 769) {
            const leftCard = document.querySelector('.panel-left .card');
            const rightCard = document.querySelector('.panel-right .card');
            if (leftCard && rightCard) {
                rightCard.style.height = leftCard.offsetHeight + 'px';
            }
        } else {
            const rightCard = document.querySelector('.panel-right .card');
            if (rightCard) {
                rightCard.style.height = '';
            }
        }
    }
    
    // Run on load and resize
    window.addEventListener('load', syncCardHeights);
    window.addEventListener('resize', syncCardHeights);
    // Also sync after image loads (may change left card height)
    const originalDrawSrc = drawSrc;
    drawSrc = function() {
        originalDrawSrc();
        setTimeout(syncCardHeights, 0);
    };
})();
