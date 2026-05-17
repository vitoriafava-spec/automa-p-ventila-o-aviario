let estado = {
  modo: "auto",
  temperatura: 25,
  rele1: false,
  rele2: false,
  rele3: false
};


function toggleRele(num) {
  if (estado.modo !== "manual") return;

  estado["rele" + num] = !estado["rele" + num];
  atualizarTela();
}


function setModo(modo) {
  estado.modo = modo;
  atualizarTela();
}


function simularTemperatura() {
  estado.temperatura += (Math.random() * 2 - 1);

  
  if (estado.modo === "auto") {
    estado.rele1 = estado.temperatura > 26;
    estado.rele2 = estado.temperatura > 28;
    estado.rele3 = estado.temperatura > 30;
  }

  atualizarTela();
}


function atualizarTela() {
  document.getElementById("temp").innerText =
    estado.temperatura.toFixed(1) + " °C";

  document.getElementById("modoStatus").innerText =
    estado.modo.toUpperCase();

  atualizarRele("r1", estado.rele1);
  atualizarRele("r2", estado.rele2);
  atualizarRele("r3", estado.rele3);
}

function atualizarRele(id, valor) {
  const el = document.getElementById(id);
  el.innerText = valor ? "ON" : "OFF";
  el.className = valor ? "ON" : "OFF";
}


setInterval(simularTemperatura, 2000);


atualizarTela();