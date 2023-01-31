'use strict';
//initiation
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const settings = document.querySelector('.settings');
const restart = document.querySelector('.restart');
const table = document.querySelector('table');
let reload_dashboard_flag = 0;
let reload_relay_flag = 0;
// Enter relay page
relay.addEventListener('click', function () {
  fetch('/dashboard')
    .then((response) => response.text())
    .then(
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

  alert('Restarting ESP32...');
  const response = await fetch('/restart', options);
  if (response.status == 200) {
    const json_data = await response.json();
    console.log(json_data);
    console.log('Restart_success_Status : ', json_data.restart_successful);
    if (json_data.restart_successful == 1) {
      location.replace('/');
      alert('Restart successful');
    }
  }
});
// Enter settings page
settings.addEventListener('click', function () {
  fetch('/dashboard')
    .then((response) => response.text())
    .then(
      window.location.replace('/settings')
    )
    .catch((error) => {
      console.error(error);
    });
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