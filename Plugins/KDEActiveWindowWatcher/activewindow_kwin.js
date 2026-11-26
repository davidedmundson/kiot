// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

var lastPayload = "";
var currentWindow = null;

function updateActiveWindow(w) {
    if (!w) return;
    if (w.transient && w.transientFor) w = w.transientFor;

    var payload = {
        title: w.caption || '',
        resourceClass: w.resourceClass || '',
        fullscreen: w.fullScreen.toString(),
        screen: w.output.manufacturer,
        x: w.x,
        y: w.y,
        width: w.width,
        height: w.height,
        pid: w.pid
    };

    var json = JSON.stringify(payload);
    if (json === lastPayload) {
        return;
    }
    lastPayload = json;
    callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateAttributes', payload);
}

function onCaptionChanged() {
    updateActiveWindow(currentWindow);
}

function onGeometryChanged() {
    updateActiveWindow(currentWindow);
}
function onFullScreenChanged() {
    updateActiveWindow(currentWindow);
}
function onOutputChanged(){
    updateActiveWindow(currentWindow);
}
function watchWindow(w) {
    if (currentWindow) {
        // the window may be gone already (closed while nothing else was active); its
        // connections went with it, and throwing here would keep currentWindow stale for good
        try {
            currentWindow.captionChanged.disconnect(onCaptionChanged);
            currentWindow.frameGeometryChanged.disconnect(onGeometryChanged);
            currentWindow.fullScreenChanged.disconnect(onFullScreenChanged);
            currentWindow.outputChanged.disconnect(onOutputChanged);
        } catch (e) {
        }
        currentWindow = null;
    }

    if (!w) return;

    currentWindow = w;
    currentWindow.captionChanged.connect(onCaptionChanged);
    currentWindow.frameGeometryChanged.connect(onGeometryChanged);
    currentWindow.fullScreenChanged.connect(onFullScreenChanged);
    currentWindow.outputChanged.connect(onOutputChanged);

    updateActiveWindow(w);
}

watchWindow(workspace.activeWindow);
workspace.windowActivated.connect(watchWindow);

