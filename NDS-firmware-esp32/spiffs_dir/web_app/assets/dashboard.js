'use strict';
//initiation
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const settings = document.querySelector('.settings');
const restart = document.querySelector('.restart');
const table = document.querySelector('table');
const home = document.querySelector('.home');
let logout = document.querySelector('.logout');
let reload_dashboard_flag = 0;
let reload_relay_flag = 0;
let time = 300000;
home.addEventListener('click', function () {
  alert('Signing out....Press "ENTER". ');
  window.location.replace('/');
});
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
  const response = await fetch('/restart', options);
  if (response.status == 200) {
    const json_data = await response.json();
    if (json_data.restart_successful == 1) {
      window.location.replace('/');
      time = 3000;

    }
    else {
      alert(`Restart unsuccessful....`);
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

window.addEventListener('mousemove', function () {
  if (time <= 20000) {
    time = 300000;
    logout.textContent = ``;
  }
});
const timer = setInterval(function () {
  const min = String(Math.trunc(time / 60000)).padStart(2, 0);
  const sec = String((time % 60000) / 1000).padStart(2, 0);
  time = time - 1000;
  if (time <= 1000) {
    clearInterval(timer);
    alert('Session Timeout......Press "ENTER".');
    window.location.replace('/');
  }
  if (time <= 10000) { logout.textContent = `User Inactive!! Timeout in... ${sec}s`; }
}, 1000);