# Responsive Design Skill

## Purpose
Master responsive web design techniques to create interfaces that work seamlessly across all devices - mobile phones, tablets, laptops, and desktops.

## When to Use
- Building mobile-friendly websites
- Creating adaptive layouts
- Optimizing for different screen sizes
- Improving mobile user experience
- Ensuring accessibility across devices

## Mobile-First Approach

### Why Mobile-First?

✅ Forces focus on essential content
✅ Faster mobile performance (no overrides)
✅ Easier to scale up than down
✅ Mobile traffic exceeds desktop globally
✅ Better for progressive enhancement

### Mobile-First CSS Structure

```css
/* Base styles (Mobile: 0-767px) */
.container {
  padding: 16px;
  width: 100%;
}

.grid {
  display: grid;
  grid-template-columns: 1fr;
  gap: 16px;
}

/* Tablet (768px+) */
@media (min-width: 768px) {
  .container {
    padding: 24px;
  }

  .grid {
    grid-template-columns: repeat(2, 1fr);
    gap: 24px;
  }
}

/* Desktop (1024px+) */
@media (min-width: 1024px) {
  .container {
    padding: 32px;
    max-width: 1200px;
    margin: 0 auto;
  }

  .grid {
    grid-template-columns: repeat(3, 1fr);
    gap: 32px;
  }
}

/* Large Desktop (1280px+) */
@media (min-width: 1280px) {
  .grid {
    grid-template-columns: repeat(4, 1fr);
  }
}
```

## Breakpoint Strategy

### Standard Breakpoints

```css
:root {
  --screen-sm: 640px;   /* Mobile landscape */
  --screen-md: 768px;   /* Tablet portrait */
  --screen-lg: 1024px;  /* Tablet landscape / Small laptop */
  --screen-xl: 1280px;  /* Desktop */
  --screen-2xl: 1536px; /* Large desktop */
}

/* Mobile */
@media (max-width: 639px) { }

/* Tablet */
@media (min-width: 640px) and (max-width: 1023px) { }

/* Desktop */
@media (min-width: 1024px) { }
```

### Custom Breakpoints

Don't just use standard breakpoints - add breakpoints where YOUR content breaks:

```css
/* Content-specific breakpoint */
@media (min-width: 900px) {
  .sidebar {
    display: block; /* Show sidebar only when space permits */
  }
}
```

### Breakpoint Best Practices

✅ Use 3-5 breakpoints maximum
✅ Test actual devices, not just browser resize
✅ Focus on content, not specific devices
✅ Consider landscape/portrait orientations
✅ Use em/rem for breakpoints (accessibility)

```css
/* Good: em-based (respects user font size) */
@media (min-width: 48em) { } /* 768px at default 16px */

/* Avoid: px-based */
@media (min-width: 768px) { }
```

## Responsive Layout Techniques

### 1. Fluid Grid with Auto-fit

```css
.responsive-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 24px;
}

/* Automatically adjusts columns based on available space */
```

### 2. Flexbox Wrapping

```css
.flex-wrap {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
}

.flex-item {
  flex: 1 1 300px; /* Grow, shrink, min-width */
}

/* Items wrap to new row when space is tight */
```

### 3. Conditional Layouts

```css
/* Mobile: Stack vertically */
.layout {
  display: flex;
  flex-direction: column;
}

/* Desktop: Side-by-side */
@media (min-width: 1024px) {
  .layout {
    flex-direction: row;
  }

  .sidebar {
    flex: 0 0 280px; /* Fixed width sidebar */
  }

  .main {
    flex: 1; /* Take remaining space */
  }
}
```

### 4. Container Queries (Modern)

```css
.card-container {
  container-type: inline-size;
}

.card {
  padding: 16px;
}

/* Adjust based on container width, not viewport */
@container (min-width: 400px) {
  .card {
    display: grid;
    grid-template-columns: 1fr 2fr;
    padding: 24px;
  }
}
```

## Responsive Typography

### Fluid Typography (clamp)

```css
h1 {
  font-size: clamp(2rem, 5vw + 1rem, 4rem);
  /* Min: 2rem, Preferred: 5vw + 1rem, Max: 4rem */
}

body {
  font-size: clamp(1rem, 0.9rem + 0.5vw, 1.125rem);
}
```

### Responsive Type Scale

```css
/* Mobile */
:root {
  --text-xs: 0.75rem;
  --text-sm: 0.875rem;
  --text-base: 1rem;
  --text-lg: 1.125rem;
  --text-xl: 1.25rem;
  --text-2xl: 1.5rem;
  --text-3xl: 1.875rem;
}

/* Desktop: Increase scale */
@media (min-width: 1024px) {
  :root {
    --text-xs: 0.8125rem;
    --text-sm: 0.9375rem;
    --text-base: 1.0625rem;
    --text-lg: 1.25rem;
    --text-xl: 1.5rem;
    --text-2xl: 2rem;
    --text-3xl: 2.5rem;
  }
}
```

### Line Height Adjustments

```css
body {
  line-height: 1.6; /* Mobile: More spacing */
}

@media (min-width: 768px) {
  body {
    line-height: 1.5; /* Desktop: Standard spacing */
  }
}
```

## Responsive Images

### 1. Fluid Images

```css
img {
  max-width: 100%;
  height: auto;
  display: block;
}
```

### 2. Responsive Background Images

```css
.hero {
  background-image: url('mobile.jpg');
  background-size: cover;
  background-position: center;
}

@media (min-width: 768px) {
  .hero {
    background-image: url('tablet.jpg');
  }
}

@media (min-width: 1024px) {
  .hero {
    background-image: url('desktop.jpg');
  }
}
```

### 3. Picture Element (Best Practice)

```html
<picture>
  <source media="(min-width: 1024px)" srcset="large.webp" type="image/webp">
  <source media="(min-width: 768px)" srcset="medium.webp" type="image/webp">
  <source media="(min-width: 1024px)" srcset="large.jpg">
  <source media="(min-width: 768px)" srcset="medium.jpg">
  <img src="small.jpg" alt="Description">
</picture>
```

### 4. Srcset (Responsive + Retina)

```html
<img
  srcset="
    image-320w.jpg 320w,
    image-640w.jpg 640w,
    image-960w.jpg 960w,
    image-1280w.jpg 1280w
  "
  sizes="
    (max-width: 640px) 100vw,
    (max-width: 1024px) 50vw,
    33vw
  "
  src="image-640w.jpg"
  alt="Description"
>
```

## Touch-Friendly Design

### Touch Target Sizes

**Minimum**: 44x44px (Apple), 48x48px (Google)
**Recommended**: 48x48px minimum

```css
.btn {
  min-width: 48px;
  min-height: 48px;
  padding: 12px 24px;
}

/* Mobile: Increase padding */
@media (max-width: 767px) {
  .btn {
    min-height: 56px;
    padding: 16px 32px;
  }
}
```

### Touch vs. Hover

```css
/* Hover effects only on devices that support hover */
@media (hover: hover) and (pointer: fine) {
  .card:hover {
    transform: translateY(-4px);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.15);
  }
}

/* Touch feedback for touch devices */
@media (hover: none) and (pointer: coarse) {
  .card:active {
    transform: scale(0.98);
    opacity: 0.9;
  }
}
```

### Prevent Touch Actions

```css
/* Disable pull-to-refresh */
body {
  overscroll-behavior-y: contain;
}

/* Disable double-tap zoom */
button, a {
  touch-action: manipulation;
}

/* Disable text selection on UI elements */
.button, .icon {
  user-select: none;
  -webkit-user-select: none;
}
```

## Navigation Patterns

### 1. Hamburger Menu (Mobile)

```html
<nav class="navbar">
  <div class="logo">Brand</div>
  <button class="menu-toggle" aria-label="Toggle menu">☰</button>
  <ul class="nav-menu">
    <li><a href="#home">Home</a></li>
    <li><a href="#about">About</a></li>
    <li><a href="#contact">Contact</a></li>
  </ul>
</nav>
```

```css
/* Mobile: Hidden menu, visible toggle */
.menu-toggle {
  display: block;
  background: none;
  border: none;
  font-size: 24px;
  cursor: pointer;
}

.nav-menu {
  position: fixed;
  top: 60px;
  left: -100%;
  width: 100%;
  background: white;
  transition: left 0.3s;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
}

.nav-menu.active {
  left: 0;
}

/* Desktop: Horizontal menu, hidden toggle */
@media (min-width: 768px) {
  .menu-toggle {
    display: none;
  }

  .nav-menu {
    position: static;
    display: flex;
    gap: 32px;
    box-shadow: none;
  }
}
```

### 2. Tab Bar (Mobile Bottom Navigation)

```css
.tab-bar {
  display: flex;
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  background: white;
  box-shadow: 0 -2px 8px rgba(0, 0, 0, 0.1);
  z-index: 100;
}

.tab-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 12px;
  text-decoration: none;
  color: #64748b;
}

.tab-item.active {
  color: #3b82f6;
}

/* Desktop: Hide tab bar */
@media (min-width: 768px) {
  .tab-bar {
    display: none;
  }
}
```

## Responsive Tables

### 1. Horizontal Scroll

```css
.table-container {
  width: 100%;
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
}

table {
  min-width: 600px; /* Prevent excessive shrinking */
}
```

### 2. Card Layout (Mobile)

```css
/* Mobile: Display as cards */
@media (max-width: 767px) {
  table, thead, tbody, th, td, tr {
    display: block;
  }

  thead {
    display: none;
  }

  tr {
    margin-bottom: 16px;
    border: 1px solid #e2e8f0;
    border-radius: 8px;
    padding: 16px;
  }

  td {
    position: relative;
    padding-left: 40%;
    text-align: right;
  }

  td::before {
    content: attr(data-label);
    position: absolute;
    left: 16px;
    font-weight: 600;
    text-align: left;
  }
}
```

```html
<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Email</th>
      <th>Status</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td data-label="Name">John Doe</td>
      <td data-label="Email">john@example.com</td>
      <td data-label="Status">Active</td>
    </tr>
  </tbody>
</table>
```

## Viewport Meta Tag

**Essential** for responsive design:

```html
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=5">
```

**Parameters**:
- `width=device-width` - Match device width
- `initial-scale=1` - No initial zoom
- `maximum-scale=5` - Allow up to 5x zoom (accessibility)
- Don't use `user-scalable=no` (bad for accessibility)

## Testing Responsive Design

### Browser DevTools
- Chrome: Ctrl+Shift+M (device toolbar)
- Firefox: Ctrl+Shift+M (responsive design mode)
- Safari: Develop > Enter Responsive Design Mode

### Real Devices
Test on actual phones/tablets whenever possible:
- iPhone (iOS Safari)
- Android phone (Chrome)
- Tablet (iPad, Android tablet)
- Different screen sizes

### Online Tools
- **BrowserStack** - Real device testing
- **Responsively App** - Multi-device preview
- **Chrome DevTools** - Device simulation
- **Sizzy** - Side-by-side device preview

## Responsive Design Checklist

### Layout
- [ ] Mobile-first approach
- [ ] Content readable without zooming
- [ ] No horizontal scrolling
- [ ] Flexible grids (%, fr, auto)
- [ ] Proper breakpoints

### Typography
- [ ] Minimum 16px body text on mobile
- [ ] Readable line length (45-75 characters)
- [ ] Adequate line-height (1.5-1.6)
- [ ] Scalable font sizes (rem/em)

### Images
- [ ] Responsive images (max-width: 100%)
- [ ] Optimized for mobile (compressed)
- [ ] Srcset for different resolutions
- [ ] Lazy loading for below-fold images

### Touch
- [ ] Touch targets minimum 48x48px
- [ ] Adequate spacing between clickable elements
- [ ] Touch feedback on interactions
- [ ] No hover-only interactions

### Navigation
- [ ] Mobile menu (hamburger or equivalent)
- [ ] Easy-to-tap nav items
- [ ] Visible current page indicator
- [ ] Accessible focus states

### Forms
- [ ] Large, touch-friendly inputs
- [ ] Appropriate input types (tel, email, etc.)
- [ ] Visible labels
- [ ] Clear error messages
- [ ] Auto-zoom disabled on inputs (16px min font)

### Performance
- [ ] Fast load time on mobile (< 3 seconds)
- [ ] Optimized images
- [ ] Minimal JavaScript
- [ ] Critical CSS inlined

### Testing
- [ ] Tested on real mobile devices
- [ ] Works on iOS Safari
- [ ] Works on Chrome Android
- [ ] Portrait and landscape orientations
- [ ] Different screen sizes (small, medium, large)

## Common Responsive Mistakes

❌ Not using viewport meta tag
❌ Fixed widths instead of flexible units
❌ Desktop-first approach
❌ Too many breakpoints
❌ Forgetting landscape orientation
❌ Tiny touch targets (< 44px)
❌ Hover-only interactions
❌ Unoptimized images
❌ Not testing on real devices
❌ Horizontal scrolling
❌ Text too small on mobile
❌ Ignoring performance on mobile networks

## Advanced Techniques

### Responsive Spacing

```css
/* Fluid spacing */
.section {
  padding: clamp(2rem, 5vw, 4rem) clamp(1rem, 3vw, 2rem);
}

/* Or responsive scale */
:root {
  --space-sm: 8px;
  --space-md: 16px;
  --space-lg: 24px;
}

@media (min-width: 768px) {
  :root {
    --space-sm: 12px;
    --space-md: 24px;
    --space-lg: 48px;
  }
}
```

### Orientation Queries

```css
/* Portrait mode */
@media (orientation: portrait) {
  .gallery {
    grid-template-columns: repeat(2, 1fr);
  }
}

/* Landscape mode */
@media (orientation: landscape) {
  .gallery {
    grid-template-columns: repeat(4, 1fr);
  }
}
```

### Responsive Utilities

```css
/* Hide on mobile */
.hidden-mobile {
  display: none;
}

@media (min-width: 768px) {
  .hidden-mobile {
    display: block;
  }
}

/* Hide on desktop */
.hidden-desktop {
  display: block;
}

@media (min-width: 768px) {
  .hidden-desktop {
    display: none;
  }
}
```

## Conclusion

Responsive design is essential in modern web development. Focus on mobile-first design, use flexible layouts, optimize for touch, and always test on real devices. Remember: responsive design isn't just about making things fit - it's about creating the best experience for each device and screen size!
