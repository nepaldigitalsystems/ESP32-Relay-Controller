'using strict';
let td1 = document.querySelector('.cm');
let td2 = document.querySelector('.ci');
let td3 = document.querySelector('.cc');
let td4 = document.querySelector('.fs');
let td5 = document.querySelector('.hs');
let td6 = document.querySelector('.fd');
let td7 = document.querySelector('.fi');
let td8 = document.querySelector('.fh');
let td9 = document.querySelector('.bc');
let td10 = document.querySelector('.ct');
let td11 = document.querySelector('.ut');
let home = document.querySelector('.home');
let logout = document.querySelector('.logout');
let time = 300000;
home.addEventListener('click', function () {
    window.location.replace('/dashboard');
});
window.addEventListener('load', async function (e) {
    e.preventDefault();
    const data = {
        InfoReq: 1,
    };
    const options = {
        method: 'POST',
        body: JSON.stringify(data),
    };
    const response = await fetch('/info_post', options);
    if (response.status == 200) {
        const json_data = await response.json();

        if (json_data.Approve == 1) {
            td1.innerHTML = json_data.CHIP_MODEL;
            td2.innerHTML = json_data.CHIP_ID_MAC;
            td3.innerHTML = json_data.CHIP_CORES;
            td4.innerHTML = json_data.FLASH_SIZE;
            td5.innerHTML = json_data.HEAP_SIZE;
            td6.innerHTML = json_data.FREE_DRAM;
            td7.innerHTML = json_data.FREE_IRAM;
            td8.innerHTML = json_data.FREE_HEAP;
            td9.innerHTML = json_data.BOOT_COUNT;
            td10.innerHTML = json_data.COMPILE_TIME;
            td11.innerHTML = json_data.UP_TIME;
        }
    }
});
window.onload;

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
    if (time === 1000) {
        clearInterval(timer);
        alert('Session Timeout...');
        window.location.replace('/');
    }
    if (time <= 10000)
        logout.textContent = `User Inactive!! Timeout in... ${sec}s`;
}, 1000);