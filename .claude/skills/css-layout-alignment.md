# CSS Layout & Alignment Skill

## Purpose
Master modern CSS layout techniques including Flexbox, Grid, positioning, and alignment strategies for creating responsive, professional web layouts.

## When to Use
- Creating page layouts and component structures
- Aligning elements horizontally and vertically
- Building responsive grid systems
- Positioning overlays, modals, tooltips
- Creating complex multi-column layouts

## CSS Layout Systems

### 1. Flexbox (1D Layout)
Perfect for: Navigation bars, card layouts, button groups, centering

#### Basic Flexbox Setup
```css
.flex-container {
  display: flex;

  /* Direction */
  flex-direction: row; /* row | row-reverse | column | column-reverse */

  /* Wrapping */
  flex-wrap: wrap; /* nowrap | wrap | wrap-reverse */

  /* Main axis alignment (horizontal in row) */
  justify-content: center; /* flex-start | flex-end | center | space-between | space-around | space-evenly */

  /* Cross axis alignment (vertical in row) */
  align-items: center; /* flex-start | flex-end | center | stretch | baseline */

  /* Multi-line alignment */
  align-content: center; /* flex-start | flex-end | center | space-between | space-around | stretch */

  /* Gap between items */
  gap: 16px; /* Modern way to add spacing */
}

.flex-item {
  /* Grow factor */
  flex-grow: 1; /* 0 = don't grow, 1+ = grow proportionally */

  /* Shrink factor */
  flex-shrink: 1; /* 0 = don't shrink, 1+ = shrink proportionally */

  /* Base size */
  flex-basis: auto; /* auto | 0 | 200px | 50% */

  /* Shorthand: flex: grow shrink basis */
  flex: 1 1 auto;

  /* Individual alignment override */
  align-self: flex-start; /* auto | flex-start | flex-end | center | baseline | stretch */
}
```

#### Common Flexbox Patterns

**Center Everything (Holy Grail)**
```css
.center-everything {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 100vh;
}
```

**Navbar Layout**
```css
.navbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 32px;
}

.nav-links {
  display: flex;
  gap: 24px;
}
```

**Card Grid with Flex**
```css
.card-container {
  display: flex;
  flex-wrap: wrap;
  gap: 24px;
}

.card {
  flex: 1 1 300px; /* Grow, shrink, min-width 300px */
}
```

**Sidebar Layout**
```css
.layout {
  display: flex;
  min-height: 100vh;
}

.sidebar {
  flex: 0 0 280px; /* Don't grow, don't shrink, fixed 280px */
}

.main-content {
  flex: 1; /* Take remaining space */
}
```

### 2. CSS Grid (2D Layout)
Perfect for: Complex layouts, dashboards, galleries, forms

#### Basic Grid Setup
```css
.grid-container {
  display: grid;

  /* Define columns */
  grid-template-columns: 200px 1fr 200px; /* Fixed | Flexible | Fixed */
  /* Or: repeat(3, 1fr) - 3 equal columns */
  /* Or: repeat(auto-fit, minmax(250px, 1fr)) - responsive */

  /* Define rows */
  grid-template-rows: auto 1fr auto; /* Header | Content | Footer */

  /* Gap between cells */
  gap: 24px; /* Or: row-gap: 16px; column-gap: 24px; */

  /* Named areas */
  grid-template-areas:
    "header header header"
    "sidebar main aside"
    "footer footer footer";
}

.grid-item {
  /* Span multiple columns */
  grid-column: 1 / 3; /* Start at 1, end at 3 */
  /* Or: grid-column: span 2; - span 2 columns */

  /* Span multiple rows */
  grid-row: 1 / 4;

  /* Named area placement */
  grid-area: header;

  /* Alignment within cell */
  justify-self: center; /* start | end | center | stretch */
  align-self: center; /* start | end | center | stretch */
}
```

#### Common Grid Patterns

**Dashboard Layout**
```css
.dashboard {
  display: grid;
  grid-template-columns: 280px 1fr;
  grid-template-rows: 60px 1fr;
  grid-template-areas:
    "sidebar header"
    "sidebar main";
  min-height: 100vh;
  gap: 0;
}

.header {
  grid-area: header;
}

.sidebar {
  grid-area: sidebar;
}

.main {
  grid-area: main;
}
```

**Responsive Gallery**
```css
.gallery {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 20px;
}

/* Each item automatically fills available space */
```

**Card Grid with Different Sizes**
```css
.card-grid {
  display: grid;
  grid-template-columns: repeat(12, 1fr); /* 12-column system */
  gap: 24px;
}

.card-small {
  grid-column: span 3; /* 25% width */
}

.card-medium {
  grid-column: span 6; /* 50% width */
}

.card-large {
  grid-column: span 12; /* 100% width */
}

.card-featured {
  grid-column: span 6;
  grid-row: span 2; /* Double height */
}
```

**Holy Grail Layout**
```css
.holy-grail {
  display: grid;
  grid-template-areas:
    "header header header"
    "nav main aside"
    "footer footer footer";
  grid-template-columns: 200px 1fr 200px;
  grid-template-rows: auto 1fr auto;
  min-height: 100vh;
}

.header { grid-area: header; }
.nav { grid-area: nav; }
.main { grid-area: main; }
.aside { grid-area: aside; }
.footer { grid-area: footer; }
```

### 3. Positioning
Control exact placement of elements

```css
/* Static (default) - normal flow */
position: static;

/* Relative - offset from normal position */
.relative {
  position: relative;
  top: 10px; /* Move 10px down from original */
  left: 20px; /* Move 20px right from original */
  /* Element still occupies original space */
}

/* Absolute - positioned relative to nearest positioned ancestor */
.absolute {
  position: absolute;
  top: 0;
  right: 0;
  /* Removed from normal flow */
}

/* Fixed - positioned relative to viewport */
.fixed {
  position: fixed;
  bottom: 20px;
  right: 20px;
  /* Stays in place on scroll */
}

/* Sticky - hybrid of relative and fixed */
.sticky {
  position: sticky;
  top: 0;
  /* Acts relative until scroll threshold, then fixed */
}
```

#### Positioning Patterns

**Overlay / Modal**
```css
.overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.5);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 1000;
}

.modal {
  position: relative;
  background: white;
  border-radius: 12px;
  padding: 32px;
  max-width: 600px;
  max-height: 80vh;
  overflow-y: auto;
}
```

**Floating Action Button**
```css
.fab {
  position: fixed;
  bottom: 24px;
  right: 24px;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  background: #667eea;
  color: white;
  border: none;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
  cursor: pointer;
  z-index: 100;
}
```

**Sticky Header**
```css
.sticky-header {
  position: sticky;
  top: 0;
  background: white;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  z-index: 10;
}
```

**Badge on Corner**
```css
.notification-icon {
  position: relative;
}

.badge {
  position: absolute;
  top: -8px;
  right: -8px;
  background: #ef4444;
  color: white;
  border-radius: 50%;
  width: 20px;
  height: 20px;
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 12px;
}
```

## Centering Techniques

### Horizontal Centering

**Block Elements**
```css
.center-block {
  margin-left: auto;
  margin-right: auto;
  /* Or: margin: 0 auto; */
  width: 80%; /* Must have width */
}
```

**Inline/Text**
```css
.center-text {
  text-align: center;
}
```

**Flex**
```css
.flex-center-h {
  display: flex;
  justify-content: center;
}
```

**Grid**
```css
.grid-center-h {
  display: grid;
  justify-items: center;
}
```

### Vertical Centering

**Flex (Best Method)**
```css
.flex-center-v {
  display: flex;
  align-items: center;
  min-height: 100vh; /* or specific height */
}
```

**Grid**
```css
.grid-center-v {
  display: grid;
  align-items: center;
  min-height: 100vh;
}
```

**Absolute Position**
```css
.absolute-center-v {
  position: absolute;
  top: 50%;
  transform: translateY(-50%);
}
```

### Both Horizontal & Vertical

**Flex (Recommended)**
```css
.flex-center-both {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 100vh;
}
```

**Grid**
```css
.grid-center-both {
  display: grid;
  place-items: center; /* Shorthand for justify-items + align-items */
  min-height: 100vh;
}
```

**Absolute + Transform**
```css
.absolute-center-both {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
}
```

## Spacing Strategies

### Margin vs Padding

**Margin**: Space OUTSIDE element (pushes away other elements)
**Padding**: Space INSIDE element (expands element size)

```css
/* Margin examples */
margin: 16px; /* All sides */
margin: 16px 24px; /* Top/Bottom Left/Right */
margin: 16px 24px 32px; /* Top Left/Right Bottom */
margin: 16px 24px 32px 8px; /* Top Right Bottom Left (clockwise) */

/* Padding follows same pattern */
padding: 16px;
```

### Spacing Scale System
```css
:root {
  --space-xs: 4px;
  --space-sm: 8px;
  --space-md: 16px;
  --space-lg: 24px;
  --space-xl: 32px;
  --space-2xl: 48px;
  --space-3xl: 64px;
}

/* Usage */
.card {
  padding: var(--space-lg);
  margin-bottom: var(--space-md);
}
```

### Gap (Modern Spacing)
```css
/* Flex gap */
.flex-container {
  display: flex;
  gap: 16px; /* Space between all items */
  row-gap: 16px; /* Vertical space only */
  column-gap: 24px; /* Horizontal space only */
}

/* Grid gap */
.grid-container {
  display: grid;
  gap: 24px;
}
```

## Responsive Layout Patterns

### Mobile-First Approach
```css
/* Base styles for mobile (0-767px) */
.container {
  padding: 16px;
}

.grid {
  display: grid;
  grid-template-columns: 1fr; /* Single column */
  gap: 16px;
}

/* Tablet (768px+) */
@media (min-width: 768px) {
  .container {
    padding: 24px;
  }

  .grid {
    grid-template-columns: repeat(2, 1fr); /* 2 columns */
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
    grid-template-columns: repeat(3, 1fr); /* 3 columns */
    gap: 32px;
  }
}
```

### Responsive Grid with Auto-fit
```css
.responsive-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 24px;
}
/* Automatically creates columns based on available space */
```

### Container Queries (Modern)
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
    padding: 24px;
    display: grid;
    grid-template-columns: 1fr 2fr;
  }
}
```

## Z-Index Management

### Stacking Context Layers
```css
:root {
  --z-base: 0;
  --z-dropdown: 100;
  --z-sticky: 200;
  --z-modal-backdrop: 1000;
  --z-modal: 1001;
  --z-popover: 1100;
  --z-tooltip: 1200;
}

.dropdown {
  z-index: var(--z-dropdown);
}

.modal-backdrop {
  z-index: var(--z-modal-backdrop);
}

.modal {
  z-index: var(--z-modal);
}
```

## Alignment Utilities

### Text Alignment
```css
.text-left { text-align: left; }
.text-center { text-align: center; }
.text-right { text-align: right; }
.text-justify { text-align: justify; }
```

### Flexbox Utilities
```css
/* Direction */
.flex-row { flex-direction: row; }
.flex-col { flex-direction: column; }

/* Justify (main axis) */
.justify-start { justify-content: flex-start; }
.justify-center { justify-content: center; }
.justify-end { justify-content: flex-end; }
.justify-between { justify-content: space-between; }
.justify-around { justify-content: space-around; }
.justify-evenly { justify-content: space-evenly; }

/* Align (cross axis) */
.items-start { align-items: flex-start; }
.items-center { align-items: center; }
.items-end { align-items: flex-end; }
.items-stretch { align-items: stretch; }

/* Self alignment */
.self-start { align-self: flex-start; }
.self-center { align-self: center; }
.self-end { align-self: flex-end; }
```

## Common Layout Patterns

### Header + Content + Footer
```css
.page {
  display: grid;
  grid-template-rows: auto 1fr auto;
  min-height: 100vh;
}

.header {
  grid-row: 1;
}

.content {
  grid-row: 2;
}

.footer {
  grid-row: 3;
}
```

### Sidebar + Main Content
```css
.layout {
  display: grid;
  grid-template-columns: 280px 1fr;
  min-height: 100vh;
}

.sidebar {
  grid-column: 1;
}

.main {
  grid-column: 2;
}

/* Responsive: stack on mobile */
@media (max-width: 767px) {
  .layout {
    grid-template-columns: 1fr;
  }

  .sidebar {
    grid-column: 1;
  }

  .main {
    grid-column: 1;
  }
}
```

### Masonry Layout (Pinterest Style)
```css
.masonry {
  columns: 4; /* Number of columns */
  column-gap: 24px;
}

.masonry-item {
  break-inside: avoid; /* Prevent breaking across columns */
  margin-bottom: 24px;
}

/* Responsive */
@media (max-width: 1024px) {
  .masonry { columns: 3; }
}

@media (max-width: 768px) {
  .masonry { columns: 2; }
}

@media (max-width: 480px) {
  .masonry { columns: 1; }
}
```

## Best Practices

✅ **Use Flexbox for 1D layouts** (rows or columns)
✅ **Use Grid for 2D layouts** (rows AND columns)
✅ **Use gap instead of margins** for spacing in flex/grid
✅ **Mobile-first responsive design** (min-width media queries)
✅ **Use semantic spacing scale** (4px, 8px, 16px, 24px, 32px)
✅ **Avoid fixed heights** - use min-height instead
✅ **Use relative units** (rem, em, %, vh/vw) over px
✅ **Keep z-index organized** with CSS variables
✅ **Test on real devices** - not just browser resize
✅ **Use CSS Grid for complex layouts** - it's powerful!

## Common Mistakes

❌ Using floats for layout (outdated)
❌ Excessive nesting of flex containers
❌ Not using semantic HTML structure
❌ Fixed widths that break on mobile
❌ Forgetting to set a width when centering with margin: auto
❌ Using position: absolute for everything
❌ Not considering content overflow
❌ Ignoring flex-shrink causing layout breaks
❌ Too many breakpoints (keep it simple)
❌ Not testing in different browsers

## Debugging Tips

```css
/* Add borders to see layout */
* {
  outline: 1px solid red;
}

/* Visualize flex/grid */
.flex-container {
  background: rgba(255, 0, 0, 0.1);
}

.flex-item {
  outline: 1px dashed blue;
}

/* Check for overflow */
* {
  overflow: hidden; /* Temporarily add to find culprit */
}
```

## Conclusion

Modern CSS layout with Flexbox and Grid provides powerful, flexible tools for creating responsive designs. Master these techniques and you can build any layout efficiently without hacks or workarounds. Always choose the right tool: Flexbox for 1D, Grid for 2D, positioning for overlays, and combine them as needed!
