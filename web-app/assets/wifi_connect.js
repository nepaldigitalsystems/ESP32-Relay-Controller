'use strict';
let form = document.querySelector('.form');
let ssid = document.querySelector('.inpssid');
let pass = document.querySelector('.inpPass');
let connect = document.querySelector('.btn');

let showPass = document.querySelector('.showPass');
showPass.addEventListener('click', function () {
  if (pass.type === 'password') {
    pass.type = 'text';
    showPass.src = './image/ey.png';
  } else {
    pass.type = 'password';
    showPass.src = './image/ey.png';
  }
});

form.addEventListener('submit', function (e) {
  e.preventDefault();
  let xhr = new XMLHttpRequest();
  xhr.open('POST', '/AP_STA_post', true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.send(
    JSON.stringify({
      local_ssid: ssid.value,
      local_pass: pass.value,
    })
  );

  alert(`Resetting your wifi. Connect your device to "${ssid.value}" network.`);
});
ssid.maxLength = '31';
pass.maxLength = '31';
