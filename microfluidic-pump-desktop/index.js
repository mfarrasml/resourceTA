const {app, BrowserWindow} = require('electron');

function boot(){
    let win = new BrowserWindow({
        width: 1000,
        height: 500
    })
    win.loadFile(`main.html`)
}

app.on('ready', boot);