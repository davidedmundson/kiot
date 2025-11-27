// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

var lastPayload = {};
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

    if (payload == lastPayload) {
        return;
    }
    lastPayload = payload;
    callDBus('org.davidedmundson.kiot.ActiveWindow', '/ActiveWindow', 'org.davidedmundson.kiot.ActiveWindow', 'UpdateAttributes', payload);
}

function onCaptionChanged() {
    updateActiveWindow(currentWindow);
}

function onGeometryChanged() {
    updateActiveWindow(currentWindow);
}

function watchWindow(w) {
    if (!w) return;

    if (currentWindow) {
        currentWindow.captionChanged.disconnect(onCaptionChanged);
        currentWindow.frameGeometryChanged.disconnect(onGeometryChanged);
    }

    currentWindow = w;
    currentWindow.captionChanged.connect(onCaptionChanged);
    currentWindow.frameGeometryChanged.connect(onGeometryChanged);

    updateActiveWindow(w);
}

watchWindow(workspace.activeWindow);
workspace.windowActivated.connect(watchWindow);

