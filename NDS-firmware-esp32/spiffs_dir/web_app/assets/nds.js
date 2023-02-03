'use strict';
let form = document.querySelector('.form');
let textFieldunameEl = document.querySelector('.txtuname');
let textFieldpwdEl = document.querySelector('.txtpwd');
let login = document.querySelector('.btn');
let showPass = document.querySelector('.showPass');
showPass.addEventListener('click', function () {
    if (textFieldpwdEl.type === 'password') {
        textFieldpwdEl.type = 'text';
    } else {
        textFieldpwdEl.type = 'password';
    }
});
textFieldunameEl.maxLength = '31';
textFieldpwdEl.maxLength = '31';
form.addEventListener('submit', async function (e) {
    e.preventDefault();
    const data = {
        username: textFieldunameEl.value,
        password: textFieldpwdEl.value,
    };
    const options = {
        method: 'POST',
        body: JSON.stringify(data),
    };
    const response = await fetch('/login_auth_post', options);
    if (response.status == 200) {
        const json_data = await response.json();
        console.log(json_data);
        console.log('Approve Status : ', json_data.approve);
        if (json_data.approve == 1) {
            window.location.replace('/dashboard');
        }
    } else {
        console.log(response.status);
        console.log(response.statusText);
        alert('Wrong username/password. Try again!!');
        window.location.reload();
    }
});