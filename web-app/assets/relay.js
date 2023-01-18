'use strict';
Relay1=
Relay2=
Relay3=
Relay4=
Relay5=
Relay6=
Relay7=
Relay8=
Relay9=
Relay10=
Relay11=
Relay12=
Relay13=
Relay14=
Relay15=
Relay16=

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
