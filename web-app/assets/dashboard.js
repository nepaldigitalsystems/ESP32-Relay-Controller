'use strict';
//initiation
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const restart = document.querySelector('.restart');
const table = document.querySelector('table');
let reload_dashboard_flag = 0;
let reload_relay_flag = 0;
td1 = document.querySelector('.cm');
td2 = document.querySelector('.ci');
td3 = document.querySelector('.cc');
td4 = document.querySelector('.fs');
td5 = document.querySelector('.hs');
td6 = document.querySelector('.fd');
td7 = document.querySelector('.fi');
td8 = document.querySelector('.fh');
td9 = document.querySelector('.bc');
td10 = document.querySelector('.ct');
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
info.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    InfoReq: 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(data),
  };
  const response = await fetch('/dashboard', options);
  if (response.status == 200) {
    const json_data = await response.json();
    console.log(json_data);
    // console.log('Approve Status : ', json_data.approve);
    if (json_data.approve == 1) {
      td1 = json_data.CHIP_MODEL;
      td2 = json_data.CHIP_ID_MAC;
      td3 = json_data.CHIP_CORES;
      td4 = json_data.FLASH_SIZE;
      td5 = json_data.HEAP_SIZE;
      td6 = json_data.FREE_DRAM;
      td7 = json_data.FREE_IRAM;
      td8 = json_data.FREE_HEAP;
      td9 = json_data.BOOT_COUNT;
      td10 = json_data.COMPILE_TIME;

      // info.classList.add('hidden');
      // alert('Restarting the system...');
      window.location.replace('http://192.168.1.100/info');
    }
  }
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
  const response = await fetch('/dashboard', options);
  if (response.status == 200) {
    const json_data = await response.json();
    console.log(json_data);
    console.log('Approve Status : ', json_data.approve);
    if (json_data.approve == 1) {
      restart.classList.add('hidden');
      alert('Restarting the system...');
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
window.addEventListener('beforeunload', async function () {
  if (reload_dashboard_flag == 1) {
    const response = await fetch('/refresh_dashboard', options);
    console.log(response.status);
    console.log(response.statusText);
    alert('Refreshing dashboard...');
    reload_dashboard_flag = 0;
  }
});
