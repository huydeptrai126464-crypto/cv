const btn = document.getElementById("toggle");
const box = document.getElementById("versions");

btn.onclick = () => {
  box.classList.toggle("hidden");
};
