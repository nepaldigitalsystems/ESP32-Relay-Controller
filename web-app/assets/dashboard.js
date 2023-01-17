'use strict';
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const restart = document.querySelector('.restart');
let reload_flag = 0;

relay.addEventListener('click', function () {
  window.location.href = 'relay.html';
});
restart.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    restart: true,
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

window.addEventListener('load', () => {
  setTimeout(() => {
    alert(
      'Warning! Session Timeout due to inactivity..... Reload to redirect into login page. '
    );
  }, 175000);
  reload_flag = 1;
});
window.onload;
window.addEventListener('beforeunload', async function () {
  if (reload_flag == 1) {
    const response = await fetch('/dashboard');
    console.log(response.status);
    console.log(response.statusText);
    alert(
      'You are being redirected to login page.... Please re-enter your credentials.'
    );
    reload_flag = 0;
  }
});
