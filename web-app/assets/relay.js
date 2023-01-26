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
let home = document.querySelector('.home');
let reload_relay_flag = 0;
let random_value = 0;

let json_data = {
  Relay1: 0,
  Relay2: 0,
  Relay3: 0,
  Relay4: 0,
  Relay5: 0,
  Relay6: 0,
  Relay7: 0,
  Relay8: 0,
  Relay9: 0,
  Relay10: 0,
  Relay11: 0,
  Relay12: 0,
  Relay13: 0,
  Relay14: 0,
  Relay15: 0,
  Relay16: 0,
  random: 0,
  serial: 0,
};
let data;
home.addEventListener('click', function () {
  window.location.replace('/dashboard');
});
Relay1.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay1: Relay1.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay1 = data.Relay1;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay1_Task_Completed : ', recv_data.Relay1_update_success);
    if (recv_data.Relay1_update_success === 1) {
      if (Relay1.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay1.style.color = 'rgb(255,255,255)';
        Relay1.style.backgroundColor = 'rgb(39, 62, 104)';
      } else {
        Relay1.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay1.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay2.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay2: Relay2.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay2 = data.Relay2;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay2_Task_Completed : ', recv_data.Relay2_update_success);
    if (recv_data.Relay2_update_success === 1) {
      if (Relay2.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay2.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay2.style.color = 'rgb(255,255,255)';
      } else {
        Relay2.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay2.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay3.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay3: Relay3.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay3 = data.Relay3;
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay3_Task_Completed : ', recv_data.Relay3_update_success);
    if (recv_data.Relay3_update_success === 1) {
      if (Relay3.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay3.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay3.style.color = 'rgb(255,255,255)';
      } else {
        Relay3.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay3.style.color = 'rgb(255,255,255)';
      }
    }
  }
  console.log(json_data);
});
Relay4.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay4: Relay4.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay4 = data.Relay4;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay4_Task_Completed : ', recv_data.Relay4_update_success);
    if (recv_data.Relay4_update_success === 1) {
      if (Relay4.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay4.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay4.style.color = 'rgb(255,255,255)';
      } else {
        Relay4.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay4.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay5.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay5: Relay5.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay5 = data.Relay5;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay5_Task_Completed : ', recv_data.Relay5_update_success);
    if (recv_data.Relay5_update_success === 1) {
      if (Relay5.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay5.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay5.style.color = 'rgb(255,255,255)';
      } else {
        Relay5.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay5.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay6.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay6: Relay6.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay6 = data.Relay6;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay6_Task_Completed : ', recv_data.Relay6_update_success);
    if (recv_data.Relay6_update_success === 1) {
      if (Relay6.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay6.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay6.style.color = 'rgb(255,255,255)';
      } else {
        Relay6.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay6.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay7.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay7: Relay7.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay7 = data.Relay7;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay7_Task_Completed : ', recv_data.Relay7_update_success);
    if (recv_data.Relay7_update_success === 1) {
      if (Relay7.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay7.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay7.style.color = 'rgb(255,255,255)';
      } else {
        Relay7.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay7.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay8.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay8: Relay8.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay8 = data.Relay8;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay8_Task_Completed : ', recv_data.Relay8_update_success);
    if (recv_data.Relay8_update_success === 1) {
      if (Relay8.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay8.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay8.style.color = 'rgb(255,255,255)';
      } else {
        Relay8.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay8.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay9.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay9: Relay9.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay9 = data.Relay9;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay9_Task_Completed : ', recv_data.Relay9_update_success);
    if (recv_data.Relay9_update_success === 1) {
      if (Relay9.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay9.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay9.style.color = 'rgb(255,255,255)';
      } else {
        Relay9.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay9.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay10.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay10: Relay10.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay10 = data.Relay10;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay10_Task_Completed : ', recv_data.Relay10_update_success);
    if (recv_data.Relay10_update_success === 1) {
      if (Relay10.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay10.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay10.style.color = 'rgb(255,255,255)';
      } else {
        Relay10.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay10.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay11.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay11: Relay11.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay11 = data.Relay11;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay11_Task_Completed : ', recv_data.Relay11_update_success);
    if (recv_data.Relay11_update_success === 1) {
      if (Relay11.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay11.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay11.style.color = 'rgb(255,255,255)';
      } else {
        Relay11.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay11.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay12.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay12: Relay12.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay12 = data.Relay12;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay12_Task_Completed : ', recv_data.Relay12_update_success);
    if (recv_data.Relay12_update_success === 1) {
      if (Relay12.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay12.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay12.style.color = 'rgb(255,255,255)';
      } else {
        Relay12.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay12.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay13.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay13: Relay13.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay13 = data.Relay13;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay13_Task_Completed : ', recv_data.Relay13_update_success);
    if (recv_data.Relay13_update_success === 1) {
      if (Relay13.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay13.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay13.style.color = 'rgb(255,255,255)';
      } else {
        Relay13.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay13.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay14.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay14: Relay14.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay14 = data.Relay14;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay14_Task_Completed : ', recv_data.Relay14_update_success);
    if (recv_data.Relay14_update_success === 1) {
      if (Relay14.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay14.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay14.style.color = 'rgb(255,255,255)';
      } else {
        Relay14.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay14.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay15.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay15: Relay15.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay15 = data.Relay15;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay15_Task_Completed : ', recv_data.Relay15_update_success);
    if (recv_data.Relay15_update_success === 1) {
      if (Relay15.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay15.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay15.style.color = 'rgb(255,255,255)';
      } else {
        Relay15.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay15.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
Relay16.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay16: Relay16.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  json_data.Relay16 = data.Relay16;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log('Relay16_Task_Completed : ', recv_data.Relay16_update_success);
    if (recv_data.Relay16_update_success === 1) {
      if (Relay16.style.backgroundColor === 'rgb(9, 247, 116)') {
        Relay16.style.backgroundColor = 'rgb(39, 62, 104)';
        Relay16.style.color = 'rgb(255,255,255)';
      } else {
        Relay16.style.backgroundColor = 'rgb(9, 247, 116)';
        Relay16.style.color = 'rgb(255,255,255)';
      }
    }
  }
});
random.innerHTML = `Random [${random_value === 0 ? 'OFF' : random_value}]`;
random.addEventListener('click', async function (e) {
  e.preventDefault();
  random_value++;
  if (random_value > 4) {
    random_value = 0;
  }
  random.innerHTML = `Random [${random_value === 0 ? 'OFF' : random_value}]`;
  console.log(random.innerHTML);
  for (const bt of btn) {
    if (random_value !== 0 && bt.value !== 'r') {
      bt.disabled = true;
      bt.classList.add('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#fff';
      if (bt.value === 's') {
        bt.style.backgroundColor = 'purple';
        bt.style.color = '#fff';
      }
    } else if (random_value === 0) {
      bt.disabled = false;
      bt.classList.remove('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#fff';
      if (bt.value === 's') {
        bt.style.backgroundColor = 'purple';
        bt.style.color = '#fff';
      }
      if (bt.value === 'r') {
        bt.style.backgroundColor = 'orange';
        bt.style.color = '#fff';
      }
    }
  }
  data = {
    random: random_value,
  };
  json_data.random = data.random;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(
      'random_update_success_Status : ',
      recv_data.random_update_success
    );
  }
});
serial.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    serial: serial.style.backgroundColor === 'rgb(9, 247, 116)' ? 0 : 1,
  };
  for (const bt of btn) {
    if (bt.value !== 's' && data.serial === 1) {
      bt.disabled = true;
      bt.classList.add('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#fff';
      if (bt.value === 'r') {
        bt.style.backgroundColor = 'orange';
        bt.style.color = '#fff';
      }
    } else if (bt.value === 's' && data.serial === 1) {
      bt.style.backgroundColor = 'rgb(9, 247, 116)';
      bt.style.color = '#fff';
    } else {
      bt.disabled = false;
      bt.classList.remove('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#fff';
      if (bt.value === 'r') {
        bt.style.backgroundColor = 'orange';
        bt.style.color = '#fff';
      }
      if (bt.value === 's') {
        bt.style.backgroundColor = 'rgb(128, 0, 128)';
        bt.style.color = '#fff';
      }
    }
  }
  json_data.serial = data.serial;
  console.log(json_data);
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(
      'serial_update_success_Status : ',
      recv_data.serial_update_success
    );
    if (recv_data.serial_update_success === 1) {
      if (serial.style.backgroundColor === 'rgb(9, 247, 116)') {
        serial.style.backgroundColor = 'rgb(9, 247, 116)';
        serial.style.color = 'rgb(255, 255, 255)';
      } else {
        serial.style.backgroundColor = 'rgb(128, 0, 128)';
        serial.style.color = 'rgb(255, 255, 255)';
      }
    }
  }
});
// window.addEventListener('load', () => {
//   // setTimeout(() => {
//   //   reload_relay_flag = 1;
//   // }, 2000);
//   setTimeout(() => {
//     alert(
//       'Warning! Session Timeout due to inactivity..... \n Reload to redirect into login page. '
//     );
//   }, 175000);
// });
// window.onload;
// window.addEventListener('beforeunload', async function () {
//   if (reload_relay_flag == 1) {
//     const response = fetch('/refresh_relay', options);
//     console.log(response.status);
//     console.log(response.statusText);
//     alert('Refreshing Relay_Page...');
//     reload_flag = 0;
//   }
// });
