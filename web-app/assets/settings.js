let form = document.querySelector('form');
let curr = document.querySelector('.currpass');
let newpass = document.querySelector('.newpass');
let conpass = document.querySelector('.conpass');
let newUser = document.querySelector('.newuser');
let showPass0 = document.querySelector('.showPass0');
let showPass1 = document.querySelector('.showPass1');
let showPass2 = document.querySelector('.showPass2');
let check = document.querySelector('.cb');
let home = document.querySelector('.home');
home.addEventListener('click', function () {
  window.location.replace('/dashboard');
});
showPass0.addEventListener('click', function () {
  if (curr.type === 'password') {
    curr.type = 'text';
    showPass0.src = './image/ey.png';
  } else {
    curr.type = 'password';
    showPass0.src = './image/ey.png';
  }
});
newUser.maxLength = '31';
newpass.maxLength = '31';
showPass1.addEventListener('click', function () {
  if (newpass.type === 'password') {
    newpass.type = 'text';
    showPass1.src = './image/ey.png';
  } else {
    newpass.type = 'password';
    showPass1.src = './image/ey.png';
  }
});
showPass2.addEventListener('click', function () {
  if (conpass.type === 'password') {
    conpass.type = 'text';
    showPass2.src = './image/ey.png';
  } else {
    conpass.type = 'password';
    showPass2.src = './image/ey.png';
  }
});
check.addEventListener('click', function () {
  newUser.classList.remove('hidden');
});
form.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    current_password: curr.value,
    new_password: newpass.value,
    confirm_password: conpass.value,
    new_username: newUser.value,
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
      window.location.replace('/');
    }
  }
});
