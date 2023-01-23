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

/* inital json_packet */
console.log(json_data);

Relay1.addEventListener('click', async function (e) {
  e.preventDefault();

  data = {
    Relay1: Relay1.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay1_update_success);
    if (recv_data.Relay1_update_success == 1) {
      Relay1.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay1 = data.Relay1;
  console.log(json_data);
});

Relay2.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay2: Relay2.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay2_update_success);
    if (recv_data.Relay2_update_success == 1) {
      Relay2.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay2 = data.Relay2;
  console.log(json_data);
});
Relay3.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay3: Relay3.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay3_update_success);
    if (recv_data.Relay3_update_success == 1) {
      Relay3.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay3 = data.Relay3;
  console.log(json_data);
});
Relay4.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay4: Relay4.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay4_update_success);
    if (recv_data.Relay4_update_success == 1) {
      Relay4.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay4 = data.Relay4;
  console.log(json_data);
});
Relay5.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay5: Relay5.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay5_update_success);
    if (recv_data.Relay5_update_success == 1) {
      Relay5.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay5 = data.Relay5;
  console.log(json_data);
});
Relay6.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay6: Relay6.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay6_update_success);
    if (recv_data.Relay6_update_success == 1) {
      Relay6.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay6 = data.Relay6;
  console.log(json_data);
});

Relay7.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay7: Relay7.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay7_update_success);
    if (recv_data.Relay7_update_success == 1) {
      Relay7.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay7 = data.Relay7;
  console.log(json_data);
});
Relay8.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay8: Relay8.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay8_update_success);
    if (recv_data.Relay8_update_success == 1) {
      Relay8.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay8 = data.Relay8;
  console.log(json_data);
});
Relay9.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay9: Relay9.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay9_update_success);
    if (recv_data.Relay9_update_success == 1) {
      Relay9.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay9 = data.Relay9;
  console.log(json_data);
});
Relay10.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay10: Relay10.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay10_update_success);
    if (recv_data.Relay10_update_success == 1) {
      Relay10.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay10 = data.Relay10;
  console.log(json_data);
});
Relay11.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay11: Relay11.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay11_update_success);
    if (recv_data.Relay11_update_success == 1) {
      Relay11.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay11 = data.Relay11;
  console.log(json_data);
});

Relay12.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay12: Relay12.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay12_update_success);
    if (recv_data.Relay12_update_success == 1) {
      Relay12.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay12 = data.Relay12;
  console.log(json_data);
});
Relay13.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay13: Relay13.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay13_update_success);
    if (recv_data.Relay13_update_success == 1) {
      Relay13.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay13 = data.Relay13;
  console.log(json_data);
});
Relay14.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay14: Relay14.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay14_update_success);
    if (recv_data.Relay14_update_success == 1) {
      Relay14.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay14 = data.Relay14;
  console.log(json_data);
});
Relay15.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay15: Relay15.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay15_update_success);
    if (recv_data.Relay15_update_success == 1) {
      Relay15.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay15 = data.Relay15;
  console.log(json_data);
});

Relay16.addEventListener('click', async function (e) {
  e.preventDefault();
  data = {
    Relay16: Relay16.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.Relay16_update_success);
    if (recv_data.Relay16_update_success == 1) {
      Relay16.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.Relay16 = data.Relay16;
  console.log(json_data);
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
      bt.style.color = '#a0a0a0';
      if (bt.value === 's') {
        bt.style.backgroundColor = 'purple';
        bt.style.color = '#fff';
      }
    } else if (random_value === 0) {
      bt.disabled = false;
      bt.classList.remove('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#a0a0a0';
      bt.addEventListener('mouseover', function () {
        bt.style.backgroundColor = '#a0a0a0';
        bt.style.color = '#273e68';
      });
      bt.addEventListener('mouseout', function () {
        bt.style.backgroundColor = '#273e68';
        bt.style.color = '#a0a0a0';
      });
      if (bt.value === 's') {
        bt.style.backgroundColor = 'purple';
        bt.style.color = '#fff';
        bt.addEventListener('mouseover', function () {
          bt.style.backgroundColor = '#a0a0a0';
          bt.style.color = '#273e68';
        });
        bt.addEventListener('mouseout', function () {
          bt.style.backgroundColor = 'purple';
          bt.style.color = '#fff';
        });
      }
      if (bt.value === 'r') {
        bt.style.backgroundColor = 'orange';
        bt.style.color = '#fff';
        bt.addEventListener('mouseover', function () {
          bt.style.backgroundColor = '#a0a0a0';
          bt.style.color = '#273e68';
        });
        bt.addEventListener('mouseout', function () {
          bt.style.backgroundColor = 'orange';
          bt.style.color = '#fff';
        });
      }
      console.log(bt);
    }
  }
  data = {
    random: random_value,
  };
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.random_update_success);
    // if (recv_data.random_update_success == 1) {
    //   random.style.backgroundColor = '#09f774';
    // }
  }
  console.log(data);
  json_data.random = data.random;
  console.log(json_data);
});
serial.addEventListener('click', async function (e) {
  e.preventDefault();

  data = {
    serial: serial.style.backgroundColor === '#09f774' ? 0 : 1,
  };
  for (const bt of btn) {
    if (bt.value !== 's' && data.serial === 1) {
      bt.disabled = true;
      bt.classList.add('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#a0a0a0';
      if (bt.value === 'r') {
        bt.style.backgroundColor = 'orange';
        bt.style.color = '#fff';
      }
    } else {
      bt.disabled = false;
      bt.classList.remove('disabled');
      bt.style.backgroundColor = '#273e68';
      bt.style.color = '#a0a0a0';
      if (bt.value === 's') {
        bt.style.backgroundColor = 'purple';
        bt.style.color = '#fff';
      }

      console.log(bt);
    }
  }
  const options = {
    method: 'POST',
    body: JSON.stringify(json_data),
  };
  const response = await fetch('/relay_json_post', options);
  if (response.status == 200) {
    const recv_data = await response.json();
    console.log(recv_data);
    console.log('Approve Status : ', recv_data.serial_update_success);
    if (recv_data.serial_update_success == 1) {
      serial.style.backgroundColor = '#09f774';
    }
  }
  console.log(data);
  json_data.serial = data.serial;
  console.log(json_data);

  // send updated json packet
});

window.addEventListener('load', () => {
  setTimeout(() => {
    reload_relay_flag = 1;
  }, 2000);
  setTimeout(() => {
    alert(
      'Warning! Session Timeout due to inactivity..... \n Reload to redirect into login page. '
    );
  }, 175000);
});
window.onload;
window.addEventListener('beforeunload', async function () {
  if (reload_relay_flag == 1) {
    const response = fetch('/refresh_relay', options);
    console.log(response.status);
    console.log(response.statusText);
    alert('Refreshing Relay_Page...');
    reload_flag = 0;
  }
});
