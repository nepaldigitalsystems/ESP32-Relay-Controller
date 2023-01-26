let form = document.querySelector('form');

form.addEventListener('click', async function (e) {
  e.preventDefault();
  const data = {
    current_password: inpcurPass.value,
    new_password: inpnewPass.value,
    confirm_password: conPass.value,
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
      window.location.replace('/settings');
    }
  }
});
