'use strict';
let btn = document.querySelectorAll('.btn');
let Relay1 = document.querySelector('.btn-1');
let Relay2 = document.querySelector('.btn-2');
let Relay3 = document.querySelector('.btn-3');
let Relay4 = document.querySelector('.btn-4');
let Relay5 = document.querySelector('.btn-5');
let Relay6 = document.querySelector('.btn-6');
let Relay7 = document.querySelector('.btn-7');
let Relay8 = document.querySelector('.btn-8');
let Relay9 = document.querySelector('.btn-9');
let Relay10 = document.querySelector('.btn-10');
let Relay11 = document.querySelector('.btn-11');
let Relay12 = document.querySelector('.btn-12');
let Relay13 = document.querySelector('.btn-13');
let Relay14 = document.querySelector('.btn-14');
let Relay15 = document.querySelector('.btn-15');
let Relay16 = document.querySelector('.btn-16');
let random = document.querySelector('.rnd');
let serial = document.querySelector('.srl');
let reload_relay_flag = 0;

for(i=1;i<17;i++)
{
  `Relay${i}`.addEventListener
}


btn.addEventListener('click', async function (e) {
 
  e.preventDefault();
  const data = {
Relay1 :a,
Relay2 :a,
Relay3 :a,
Relay4 :a,
Relay5 :a,
Relay6 :a,
Relay7 :a,
Relay8 :a,
Relay9 :a,
Relay10:a,
Relay11:a,
Relay12:a,
Relay13:a,
Relay14:a,
Relay15:a,
Relay16:a,
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
