
const menuButton=document.querySelector('.menu-btn');
const navLinks=document.querySelector('.nav-links');
menuButton?.addEventListener('click',()=>navLinks.classList.toggle('open'));
document.querySelectorAll('.nav-links a').forEach(a=>a.addEventListener('click',()=>navLinks.classList.remove('open')));

const visual=document.querySelector('.visual-card');
if(visual && matchMedia('(pointer:fine)').matches){
  visual.addEventListener('pointerenter',()=>{
    visual.classList.add('is-interacting');
  });
  visual.addEventListener('pointermove',e=>{
    const r=visual.getBoundingClientRect();
    const x=(e.clientX-r.left)/r.width-.5;
    const y=(e.clientY-r.top)/r.height-.5;
    visual.style.transform=`perspective(1300px) translateY(-4px) rotateX(${-y*10}deg) rotateY(${x*12}deg) scale(1.01)`;
  });
  visual.addEventListener('pointerleave',()=>{
    visual.style.transform='';
    visual.classList.remove('is-interacting');
  });
}
const observer=new IntersectionObserver(entries=>{
  entries.forEach(e=>{if(e.isIntersecting)e.target.classList.add('in')});
},{threshold:.12});
document.querySelectorAll('.reveal').forEach(el=>observer.observe(el));
document.querySelector('[data-year]').textContent=new Date().getFullYear();


const galleryStage=document.querySelector('.gallery-stage');
const galleryRail=document.querySelector('.gallery-rail');
const gallerySlides=[...document.querySelectorAll('.gallery-slide')];
const galleryThumbs=[...document.querySelectorAll('.gallery-thumb')];
const galleryCurrent=document.querySelector('#gallery-current');
const galleryProgress=document.querySelector('.gallery-progress');
const galleryPrev=document.querySelector('.gallery-arrow.prev');
const galleryNext=document.querySelector('.gallery-arrow.next');

if(galleryStage && gallerySlides.length){
  let galleryIndex=0;
  let galleryTimerId=null;
  let galleryPaused=false;
  let touchStartX=0;
  const galleryDelay=5000;

  const restartProgress=()=>{
    if(!galleryProgress) return;
    galleryProgress.classList.remove('running');
    void galleryProgress.offsetWidth;
    if(!galleryPaused && !matchMedia('(prefers-reduced-motion: reduce)').matches){
      galleryProgress.classList.add('running');
    }
  };

  const showGallerySlide=(nextIndex,manual=false)=>{
    const oldIndex=galleryIndex;
    galleryIndex=(nextIndex+gallerySlides.length)%gallerySlides.length;

    gallerySlides.forEach((slide,i)=>{
      slide.classList.remove('active','previous');
      if(i===oldIndex && i!==galleryIndex) slide.classList.add('previous');
      if(i===galleryIndex) slide.classList.add('active');
    });

    galleryThumbs.forEach((thumb,i)=>thumb.classList.toggle('active',i===galleryIndex));
    galleryCurrent.textContent=String(galleryIndex+1).padStart(2,'0');

    const activeThumb=galleryThumbs[galleryIndex];
    if(activeThumb && galleryRail && innerWidth<=760){
      const targetLeft=
        activeThumb.offsetLeft -
        (galleryRail.clientWidth-activeThumb.clientWidth)/2;

      galleryRail.scrollTo({
        left:Math.max(0,targetLeft),
        behavior:manual?'smooth':'auto'
      });
    }

    restartProgress();
    if(manual) restartGalleryTimer();
  };

  const nextGallerySlide=()=>showGallerySlide(galleryIndex+1);
  const prevGallerySlide=()=>showGallerySlide(galleryIndex-1);

  const restartGalleryTimer=()=>{
    clearInterval(galleryTimerId);
    if(!galleryPaused && !matchMedia('(prefers-reduced-motion: reduce)').matches){
      galleryTimerId=setInterval(nextGallerySlide,galleryDelay);
    }
    restartProgress();
  };

  galleryThumbs.forEach((thumb,i)=>thumb.addEventListener('click',()=>showGallerySlide(i,true)));
  galleryPrev?.addEventListener('click',()=>{prevGallerySlide();restartGalleryTimer()});
  galleryNext?.addEventListener('click',()=>{nextGallerySlide();restartGalleryTimer()});

  galleryStage.addEventListener('mouseenter',()=>{galleryPaused=true;clearInterval(galleryTimerId);galleryProgress?.classList.remove('running')});
  galleryStage.addEventListener('mouseleave',()=>{galleryPaused=false;restartGalleryTimer()});
  galleryStage.addEventListener('focusin',()=>{galleryPaused=true;clearInterval(galleryTimerId);galleryProgress?.classList.remove('running')});
  galleryStage.addEventListener('focusout',()=>{galleryPaused=false;restartGalleryTimer()});

  galleryStage.addEventListener('touchstart',e=>{touchStartX=e.changedTouches[0].clientX},{passive:true});
  galleryStage.addEventListener('touchend',e=>{
    const dx=e.changedTouches[0].clientX-touchStartX;
    if(Math.abs(dx)>45){
      dx<0?nextGallerySlide():prevGallerySlide();
      restartGalleryTimer();
    }
  },{passive:true});

  document.addEventListener('visibilitychange',()=>{
    if(document.hidden){clearInterval(galleryTimerId)}
    else{restartGalleryTimer()}
  });

  showGallerySlide(0);
  restartGalleryTimer();
}

document.querySelectorAll('esp-web-install-button').forEach(button=>{
  button.addEventListener('click',()=>{
    document.querySelector('#start')?.classList.add('install-attention');
    setTimeout(()=>document.querySelector('#start')?.classList.remove('install-attention'),900);
  });
});
