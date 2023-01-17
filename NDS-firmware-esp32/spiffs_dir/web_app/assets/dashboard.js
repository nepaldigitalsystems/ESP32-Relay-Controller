'use strict';
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const restart = document.querySelector('.restart');
let reload_flag = 0;

relay.addEventListener('click', function () {
    window.location.href = 'relay.html';
});





window.addEventListener('load', () => {
    setTimeout(() => {
        alert('Warning! Session Timeout due to inactivity..... Reload to redirect into login page. ');
        reload_flag = 1
    }, 175000);
});
window.onload;
window.addEventListener('beforeunload', async function () {
    if (reload_flag == 1) {
        const response = await fetch('/dashboard');
        console.log(response.status);
        console.log(response.statusText);
        alert("You are being redirected to login page.... Please re-enter your credentials.");
        reload_flag = 0;
    }
});