'use strict';
//initiation
const relay = document.querySelector('.relay');
const info = document.querySelector('.info');
const restart = document.querySelector('.restart');
let reload_dashboard_flag = 0;

//relay button onclick function
relay.addEventListener('click', function () {
  let xhr = new XMLHttpRequest();
  xhr.open('GET', '/relay', true);
  xhr.send();
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
    // alert(
    //   //'Warning! Session Timeout due to inactivity..... Reload to redirect into login page. '
    //   'Now you can reload.'
    // );
  }, 2000);
  reload_dashboard_flag = 1;
  
});
window.onload;
window.addEventListener('beforeunload', async function () {
  if (reload_dashboard_flag == 1) {
    const response = await fetch('/refresh_dashboard', options);
    console.log(response.status);
    console.log(response.statusText);
    alert('Refreshing dashboard...');
    reload_flag = 0;
  }

 
});
