const ResizeDirective = {
  mounted(el, binding) {
    const callback = typeof binding.value === 'function' ? binding.value : null;
    el.__amcResizeCallback__ = callback;

    if (!callback || typeof ResizeObserver === 'undefined') {
      return;
    }

    const observer = new ResizeObserver(() => {
      if (el.__amcResizeCallback__) {
        el.__amcResizeCallback__();
      }
    });

    el.__amcResizeObserver__ = observer;
    observer.observe(el);
    callback();
  },

  updated(el, binding) {
    el.__amcResizeCallback__ = typeof binding.value === 'function' ? binding.value : null;
  },

  unmounted(el) {
    if (el.__amcResizeObserver__) {
      el.__amcResizeObserver__.disconnect();
      delete el.__amcResizeObserver__;
    }
    delete el.__amcResizeCallback__;
  }
};

export default ResizeDirective;
