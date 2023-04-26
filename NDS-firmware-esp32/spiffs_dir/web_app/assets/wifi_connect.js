'use strict';
let form = document.querySelector('.form');
let ssid = document.querySelector('.inpssid');
let pass = document.querySelector('.inpPass');
let connect = document.querySelector('.btn');

let showPass = document.querySelector('.showPass');
showPass.addEventListener('click', function () {
  if (pass.type === 'password') {
    pass.type = 'text';

  } else {
    pass.type = 'password';

  }
});
ssid.maxLength = '31';
pass.maxLength = '31';

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
  const response = await fetch('/AP_STA_post', options);
  if (response.status == 200) {
    const json_data = await response.json();
    if (json_data.IP_addr3 >= 0) {
      alert(
        `Resetting your wifi. Connect your device to "${ssid.value}" network. \n Your NDS_IP is 192.168.${json_data.IP_addr3 == 0 ? 'X' : json_data.IP_addr3}.${json_data.IP_addr4 == 0 ? 'X' : json_data.IP_addr4}`
      );
      window.location.reload;
    }
  }
});