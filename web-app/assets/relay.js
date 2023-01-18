'use strict';
Relay1 = document.querySelector('.btn-1');
Relay2 = document.querySelector('.btn-2');
Relay3 = document.querySelector('.btn-3');
Relay4 = document.querySelector('.btn-4');
Relay5 = document.querySelector('.btn-5');
Relay6 = document.querySelector('.btn-6');
Relay7 = document.querySelector('.btn-7');
Relay8 = document.querySelector('.btn-8');
Relay9 = document.querySelector('.btn-9');
Relay10 = document.querySelector('.btn-10');
Relay11 = document.querySelector('.btn-11');
Relay12 = document.querySelector('.btn-12');
Relay13 = document.querySelector('.btn-13');
Relay14 = document.querySelector('.btn-14');
Relay15 = document.querySelector('.btn-15');
Relay16 = document.querySelector('.btn-16');

let reload_relay_flag = 0;
restart.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    restart: 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(data),
  };
  const response = await fetch('/dashboard', options);
  if (response.status == 200) {
    const json_data = await response.json();
    console.log(json_data);
    console.log('Approve Status : ', json_data.approve);
    if (json_data.approve == 1) {
      restart.classList.add('hidden');
      alert('Restarting the system...');
      window.location.replace('http://192.168.1.100/nds');
    }
  }
});
//window onload function
window.addEventListener('load', () => {
  setTimeout(() => {
    // alert(
    //   //'Warning! Session Timeout due to inactivity..... Reload to redirect into login page. '
    //   'Now you can reload.'
    // );
  }, 2000);
  reload_relay_flag = 1;
});
window.onload;

if (reload_relay_flag == 1) {
  const response = await fetch('/refresh_relay', options);
  console.log(response.status);
  console.log(response.statusText);
  alert('Refreshing Relay_Page...');
  reload_flag = 0;
}
