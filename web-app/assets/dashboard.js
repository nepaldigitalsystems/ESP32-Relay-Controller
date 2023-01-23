'use strict';
//initiation
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const restart = document.querySelector('.restart');
const table = document.querySelector('table');
let reload_dashboard_flag = 0;
let reload_relay_flag = 0;
let td1 = document.querySelector('.cm');
let td2 = document.querySelector('.ci');
let td3 = document.querySelector('.cc');
let td4 = document.querySelector('.fs');
let td5 = document.querySelector('.hs');
let td6 = document.querySelector('.fd');
let td7 = document.querySelector('.fi');
let td8 = document.querySelector('.fh');
let td9 = document.querySelector('.bc');
let td10 = document.querySelector('.ct');
//relay button onclick function
relay.addEventListener('click', function () {
  fetch('http://192.168.1.100/relay')
    .then((response) => response.text())
    .then(
      // Do something with the HTML content here
      window.location.replace('http://192.168.1.100/relay')
    )
    .catch((error) => {
      console.error(error);
    });
});
//info button onclick function
info.addEventListener('click', function () {
  fetch('http://192.168.1.100/info')
    .then((response) => response.text())
    .then(
      // Do something with the HTML content here
      window.location.replace('http://192.168.1.100/info')
    )
    .catch((error) => {
      console.error(error);
    });
});
//restart button onclick function
restart.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    restart: 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(data),
  };
  const response = await fetch('/restart', options);
  if (response.status == 200) {
    const json_data = await response.json();
    console.log(json_data);
    console.log('Approve Status : ', json_data.approve);
    if (json_data.approve == 1) {
      // restart.classList.add('hidden');
      // alert('Restarting the system...');
      window.location.replace('http://192.168.1.100/');
    }
  }
});
//window onload function
window.addEventListener('load', () => {
  setTimeout(() => {
    reload_dashboard_flag = 1;
  }, 5000);
  setTimeout(() => {
    alert(
      'Warning! Session Timeout due to inactivity..... \n Reload to redirect into login page. '
    );
  }, 175000);
});
window.onload;
// window.addEventListener('beforeunload', async function () {
//   if (reload_dashboard_flag == 1) {
//     const response = await fetch('/refresh_dashboard', options);
//     console.log(response.status);
//     console.log(response.statusText);
//     alert('Refreshing dashboard...');
//     reload_dashboard_flag = 0;
//   }
// });
