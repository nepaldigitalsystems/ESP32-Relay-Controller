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



  form.addEventListener('submit', async function (e) {
    e.preventDefault();
    const data = {
      local_ssid: ssid.value,
      local_pass: pass.value,
    };
    const options = {
      method: 'POST',
      body: JSON.stringify(data),
    };
    const response = await fetch('/', options);
    if (response.status == 200) {
      const json_data = await response.json();
      console.log(json_data);
      console.log('Approve Status : ', json_data.approve);
      if (json_data.approve == 1) {
        
        window.location.replace('/');
      }
    }
  });
  alert(`Resetting your wifi. Connect your device to "${ssid.value}" network.`);
  alert('I love u');
    location.replace('/alert_connect_nds');

ssid.maxLength = '31';
pass.maxLength = '31';
