let form = document.querySelector('form');
let curr = document.querySelector('.currpass');
let newpass = document.querySelector('.newpass');
let conpass = document.querySelector('.conpass');
let newUser = document.querySelector('.uname');
let user_name = document.querySelector('.newuser');
let showPass0 = document.querySelector('.showPass0');
let showPass1 = document.querySelector('.showPass1');
let showPass2 = document.querySelector('.showPass2');
let check = document.querySelector('.cb');
let home = document.querySelector('.home');
let pass_warn = document.querySelector('.incpw');
let info = document.querySelector('.info');
let time = 300000;
newUser.maxLength = '31';
user_name.maxLength = '31';
curr.maxLength = '31';
conpass.maxLength = '31';
newUser.minLength = '8';
user_name.minLength = '8';
curr.minLength = '8';
conpass.minLength = '8';

window.addEventListener('load', function () {
    check.checked = false;
});
home.addEventListener('click', function () {
    window.location.replace('/dashboard');
});
showPass0.addEventListener('click', function () {
    if (curr.type === 'password') {
        curr.type = 'text';
    } else {
        curr.type = 'password';
    }
});
showPass1.addEventListener('click', function () {
    if (newpass.type === 'password') {
        newpass.type = 'text';
    } else {
        newpass.type = 'password';
    }
});
showPass2.addEventListener('click', function () {
    if (conpass.type === 'password') {
        conpass.type = 'text';
    } else {
        conpass.type = 'password';
    }
});
check.addEventListener('click', function () {
    user_name.required = true;
    newUser.classList.toggle('hidden');
    info.classList.toggle('hidden');
});

form.addEventListener('submit', async function (e) {
    e.preventDefault();
    if (newpass.value !== conpass.value) {
        newpass.value = '';
        conpass.value = '';
        pass_warn.classList.remove('hidden');
    } else {
        const data = {
            current_password: curr.value,
            new_password: newpass.value,
            confirm_password: conpass.value,
            new_username: user_name.value ? user_name.value : 'adminuser',
        };
        const options = {
            method: 'POST',
            body: JSON.stringify(data),
        };
        const response = await fetch('/settings_post', options);
        if (response.status == 200) {
            const json_data = await response.json();
            console.log(json_data);
            console.log('password_set_success : ', json_data.password_set_success);
            if (json_data.password_set_success == 1) {
                alert('New username & password set : Successful');
                window.location.replace('/');
            } else {
                alert('New username & password set : Unsuccessful');
                window.location.reload;
            }
        }
    }
});

window.addEventListener('mousemove', function () {
    if (time <= 10000) {
        time = 300000;
    }
});
const timer = setInterval(function () {
    const min = String(Math.trunc(time / 60)).padStart(2, 0);
    const sec = String(time % 60).padStart(2, 0);
    time = time - 1000;
    if (time === 1000) {
        clearInterval(timer);
        alert('Session Timeout...');
        window.location.replace('/');
    }
}, 1000);