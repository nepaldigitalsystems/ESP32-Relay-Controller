'use strict';
//initiation
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const settings = document.querySelector('.settings');
const restart = document.querySelector('.restart');
const table = document.querySelector('table');
let reload_dashboard_flag = 0;
let reload_relay_flag = 0;
relay.addEventListener('click', function () {
  fetch('/relay')
    .then((response) => response.text())
    .then(
      // Do something with the HTML content here
      window.location.replace('/relay')
    )
    .catch((error) => {
      console.error(error);
    });
});
//info button onclick function
info.addEventListener('click', function () {
  fetch('/dashboard')
    .then((response) => response.text())
    .then(
      // Do something with the HTML content here
      window.location.replace('/info')
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
      window.location.replace('/');
    }
  }
});
settings.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    settings: 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(data),
  };
  const response = await fetch('/settings_post', options);
  if (response.status == 200) {
    const json_data = await response.json();
    console.log(json_data);
    console.log('Approve Status : ', json_data.approve);
    if (json_data.approve == 1) {
      window.location.replace('/settings');
    }
  }
});
//window onload function
// window.addEventListener('load', () => {
//   // setTimeout(() => {
//   //   reload_dashboard_flag = 1;
//   // }, 5000);
//   setTimeout(() => {
//     alert(
//       'Warning! Session Timeout due to inactivity..... \n Reload to redirect into login page. '
//     );
//   }, 175000);
// });
// window.onload;
// window.addEventListener('beforeunload', async function () {
//   if (reload_dashboard_flag == 1) {
//     const response = await fetch('/refresh_dashboard', options);
//     console.log(response.status);
//     console.log(response.statusText);
//     alert('Refreshing dashboard...');
//     reload_dashboard_flag = 0;
//   }
// });
