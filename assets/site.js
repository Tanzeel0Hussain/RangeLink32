
const menuButton=document.querySelector('.menu-btn');
const navLinks=document.querySelector('.nav-links');
menuButton?.addEventListener('click',()=>navLinks.classList.toggle('open'));
document.querySelectorAll('.nav-links a').forEach(a=>a.addEventListener('click',()=>navLinks.classList.remove('open')));

const visual=document.querySelector('.visual-card');
if(visual && matchMedia('(pointer:fine)').matches){
  visual.addEventListener('pointermove',e=>{
    const r=visual.getBoundingClientRect();
    const x=(e.clientX-r.left)/r.width-.5;
    const y=(e.clientY-r.top)/r.height-.5;
    visual.style.transform=`rotateX(${-y*7}deg) rotateY(${x*9}deg) translateZ(0)`;
  });
  visual.addEventListener('pointerleave',()=>visual.style.transform='rotateX(0deg) rotateY(0deg)');
}
const observer=new IntersectionObserver(entries=>{
  entries.forEach(e=>{if(e.isIntersecting)e.target.classList.add('in')});
},{threshold:.12});
document.querySelectorAll('.reveal').forEach(el=>observer.observe(el));
document.querySelector('[data-year]').textContent=new Date().getFullYear();
