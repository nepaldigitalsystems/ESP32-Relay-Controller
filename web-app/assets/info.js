'using strict';
info.addEventListener('load', async function (e) {
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
// window.location.reload;
      // info.classList.add('hidden');
      // alert('Restarting the system...');
    //   window.location.replace('http://192.168.1.100/info');
    }
  }
});
