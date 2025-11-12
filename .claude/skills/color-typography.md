# Color Theory & Typography Skill

## Purpose
Master color theory, typography, and their application in web design to create visually appealing, readable, and accessible interfaces.

## When to Use
- Choosing color palettes for projects
- Creating brand identities
- Improving readability and accessibility
- Establishing visual hierarchy
- Designing professional interfaces

## Color Theory Fundamentals

### Color Wheel Basics

**Primary Colors**: Red, Blue, Yellow
**Secondary Colors**: Orange, Green, Purple (mix of primaries)
**Tertiary Colors**: Mix of primary and secondary

### Color Relationships

#### 1. **Complementary**
Colors opposite on color wheel
```
Blue + Orange
Red + Green
Yellow + Purple
```
**Use**: High contrast, call-to-action buttons

#### 2. **Analogous**
Colors next to each other
```
Blue + Blue-Green + Green
Red + Red-Orange + Orange
```
**Use**: Harmonious, natural-looking designs

#### 3. **Triadic**
Three colors evenly spaced
```
Red + Yellow + Blue
Orange + Green + Purple
```
**Use**: Vibrant, balanced designs

#### 4. **Split-Complementary**
Base color + two adjacent to complement
```
Blue + Red-Orange + Yellow-Orange
```
**Use**: Less tension than complementary, still vibrant

#### 5. **Monochromatic**
Single hue with different shades/tints
```
Light Blue + Medium Blue + Dark Blue
```
**Use**: Elegant, cohesive designs

## Color Systems for Web

### 1. HSL (Hue, Saturation, Lightness)
**Best for**: Creating color variations programmatically

```css
:root {
  /* Base color */
  --primary-h: 217; /* Hue: 0-360 */
  --primary-s: 91%;  /* Saturation: 0-100% */
  --primary-l: 60%;  /* Lightness: 0-100% */

  --primary: hsl(var(--primary-h), var(--primary-s), var(--primary-l));
  --primary-light: hsl(var(--primary-h), var(--primary-s), 75%);
  --primary-dark: hsl(var(--primary-h), var(--primary-s), 45%);
}
```

### 2. RGB/RGBA
**Best for**: Transparency effects

```css
.overlay {
  background: rgba(0, 0, 0, 0.5); /* Black at 50% opacity */
}
```

### 3. Hex Colors
**Best for**: Standard solid colors

```css
:root {
  --primary: #3b82f6;
  --secondary: #8b5cf6;
}
```

## Color Palette Creation

### The 60-30-10 Rule

**60%**: Dominant color (backgrounds, large areas)
**30%**: Secondary color (supporting elements)
**10%**: Accent color (CTAs, highlights)

```css
:root {
  /* 60% - Dominant */
  --bg-primary: #f8fafc;
  --bg-secondary: #f1f5f9;

  /* 30% - Secondary */
  --text-primary: #1e293b;
  --text-secondary: #64748b;

  /* 10% - Accent */
  --accent: #3b82f6;
  --accent-hover: #2563eb;
}
```

### Professional Color Palettes

#### 1. **Corporate Blue**
```css
:root {
  --primary: #1e40af;     /* Deep blue */
  --secondary: #3b82f6;   /* Bright blue */
  --accent: #06b6d4;      /* Cyan */
  --success: #10b981;     /* Green */
  --warning: #f59e0b;     /* Amber */
  --error: #ef4444;       /* Red */
  --bg: #f8fafc;          /* Off-white */
  --text: #1e293b;        /* Dark gray */
}
```

#### 2. **Vibrant Purple**
```css
:root {
  --primary: #7c3aed;     /* Deep purple */
  --secondary: #a78bfa;   /* Light purple */
  --accent: #ec4899;      /* Pink */
  --success: #34d399;     /* Emerald */
  --warning: #fbbf24;     /* Yellow */
  --error: #f87171;       /* Coral */
  --bg: #faf5ff;          /* Light purple bg */
  --text: #1f2937;        /* Charcoal */
}
```

#### 3. **Nature Green**
```css
:root {
  --primary: #059669;     /* Green */
  --secondary: #10b981;   /* Emerald */
  --accent: #14b8a6;      /* Teal */
  --success: #22c55e;     /* Lime */
  --warning: #f59e0b;     /* Amber */
  --error: #dc2626;       /* Red */
  --bg: #f0fdf4;          /* Light green */
  --text: #064e3b;        /* Dark green */
}
```

#### 4. **Industrial Gray**
```css
:root {
  --primary: #334155;     /* Slate */
  --secondary: #64748b;   /* Gray */
  --accent: #0ea5e9;      /* Sky blue */
  --success: #22c55e;     /* Green */
  --warning: #f97316;     /* Orange */
  --error: #dc2626;       /* Red */
  --bg: #f8fafc;          /* Light gray */
  --text: #0f172a;        /* Almost black */
}
```

### Semantic Colors

Always define semantic colors for consistent meaning:

```css
:root {
  /* Status Colors */
  --success: #10b981;     /* Green - positive actions */
  --warning: #f59e0b;     /* Amber - caution */
  --error: #ef4444;       /* Red - errors/danger */
  --info: #3b82f6;        /* Blue - information */

  /* Background Levels */
  --bg-primary: #ffffff;
  --bg-secondary: #f8fafc;
  --bg-tertiary: #f1f5f9;

  /* Text Hierarchy */
  --text-primary: #1e293b;     /* Headings, important */
  --text-secondary: #475569;   /* Body text */
  --text-tertiary: #94a3b8;    /* Labels, captions */
  --text-disabled: #cbd5e1;    /* Disabled state */

  /* Border Colors */
  --border-light: #f1f5f9;
  --border-medium: #e2e8f0;
  --border-dark: #cbd5e1;
}
```

## Color Accessibility

### WCAG Contrast Requirements

**Level AA (Minimum)**:
- Normal text (< 18px): 4.5:1 contrast
- Large text (≥ 18px or ≥ 14px bold): 3:1 contrast

**Level AAA (Enhanced)**:
- Normal text: 7:1 contrast
- Large text: 4.5:1 contrast

### Testing Contrast

```css
/* GOOD - Passes AA */
.text-on-light {
  color: #1e293b;          /* Dark gray */
  background: #ffffff;     /* White */
  /* Contrast: 15.8:1 ✓ */
}

/* BAD - Fails AA */
.text-on-light-bad {
  color: #cbd5e1;          /* Light gray */
  background: #ffffff;     /* White */
  /* Contrast: 1.6:1 ✗ */
}

/* GOOD - High contrast button */
.btn-primary {
  color: #ffffff;          /* White */
  background: #1e40af;     /* Deep blue */
  /* Contrast: 10.3:1 ✓ */
}
```

### Color Blindness Considerations

**Types**:
- Protanopia (red-blind) - 1% males
- Deuteranopia (green-blind) - 1% males
- Tritanopia (blue-blind) - Very rare
- Achromatopsia (total color blindness) - Extremely rare

**Best Practices**:
- Don't rely on color alone (use icons, labels)
- Use patterns/textures in addition to color
- Test with color blindness simulators
- Ensure sufficient contrast
- Avoid red/green combinations alone

```css
/* BAD - Only color indicates status */
.success { color: green; }
.error { color: red; }

/* GOOD - Icon + color indicates status */
.success::before {
  content: '✓';
  color: #10b981;
}
.error::before {
  content: '✗';
  color: #ef4444;
}
```

## Dark Mode

### Implementing Dark Mode

```css
:root {
  /* Light mode (default) */
  --bg-primary: #ffffff;
  --bg-secondary: #f8fafc;
  --text-primary: #1e293b;
  --text-secondary: #64748b;
  --border: #e2e8f0;
}

/* Dark mode */
@media (prefers-color-scheme: dark) {
  :root {
    --bg-primary: #0f172a;
    --bg-secondary: #1e293b;
    --text-primary: #f1f5f9;
    --text-secondary: #cbd5e1;
    --border: #334155;
  }
}

/* Manual toggle */
[data-theme="dark"] {
  --bg-primary: #0f172a;
  --bg-secondary: #1e293b;
  --text-primary: #f1f5f9;
  --text-secondary: #cbd5e1;
  --border: #334155;
}
```

### Dark Mode Best Practices

✅ Use true blacks (#000000) sparingly - causes eye strain
✅ Use dark grays (#0f172a, #1e293b) for backgrounds
✅ Reduce white brightness (#f1f5f9 instead of #ffffff)
✅ Increase elevation with lighter backgrounds, not shadows
✅ Reduce saturation of colors in dark mode
✅ Test contrast ratios in both modes

---

# Typography

## Font Selection

### Font Categories

#### 1. **Serif**
Traditional, elegant, trustworthy
```css
font-family: Georgia, 'Times New Roman', serif;
```
**Use**: Editorial content, luxury brands, formal documents

#### 2. **Sans-Serif**
Modern, clean, friendly
```css
font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
```
**Use**: UI elements, body text, modern brands

#### 3. **Monospace**
Technical, code-like
```css
font-family: 'Courier New', Consolas, Monaco, monospace;
```
**Use**: Code snippets, technical documentation

#### 4. **Display**
Unique, attention-grabbing
```css
font-family: 'Orbitron', 'Audiowide', cursive;
```
**Use**: Headlines, logos, hero sections

### System Font Stack

Fast-loading, native fonts:

```css
:root {
  /* Modern system fonts */
  --font-sans: -apple-system, BlinkMacSystemFont, 'Segoe UI',
               Roboto, 'Helvetica Neue', Arial, sans-serif;

  /* Monospace for code */
  --font-mono: 'SF Mono', Monaco, 'Cascadia Code', 'Consolas',
               'Courier New', monospace;
}

body {
  font-family: var(--font-sans);
}

code, pre {
  font-family: var(--font-mono);
}
```

### Web Fonts (Google Fonts)

```html
<!-- Load fonts -->
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=Poppins:wght@700;900&display=swap" rel="stylesheet">
```

```css
:root {
  --font-body: 'Inter', sans-serif;
  --font-heading: 'Poppins', sans-serif;
}

body {
  font-family: var(--font-body);
}

h1, h2, h3, h4, h5, h6 {
  font-family: var(--font-heading);
}
```

## Type Scale

### Modular Scale
Use mathematical ratios for harmonious sizing:

**Common Scales**:
- 1.125 (Major Second)
- 1.200 (Minor Third)
- 1.250 (Major Third) ⭐ Recommended
- 1.333 (Perfect Fourth)
- 1.414 (Augmented Fourth)
- 1.500 (Perfect Fifth)
- 1.618 (Golden Ratio)

```css
:root {
  --scale: 1.250; /* Major Third */

  /* Base size */
  --text-base: 16px;

  /* Scale down */
  --text-xs: calc(var(--text-base) / var(--scale) / var(--scale));   /* 10px */
  --text-sm: calc(var(--text-base) / var(--scale));                  /* 13px */

  /* Base */
  --text-md: var(--text-base);                                        /* 16px */

  /* Scale up */
  --text-lg: calc(var(--text-base) * var(--scale));                  /* 20px */
  --text-xl: calc(var(--text-base) * var(--scale) * var(--scale));   /* 25px */
  --text-2xl: calc(var(--text-base) * var(--scale) * var(--scale) * var(--scale)); /* 31px */
  --text-3xl: calc(var(--text-base) * var(--scale) * var(--scale) * var(--scale) * var(--scale)); /* 39px */
}

/* Usage */
h1 { font-size: var(--text-3xl); }
h2 { font-size: var(--text-2xl); }
h3 { font-size: var(--text-xl); }
body { font-size: var(--text-md); }
small { font-size: var(--text-sm); }
```

### Fixed Type Scale (Tailwind-style)

```css
:root {
  --text-xs: 0.75rem;     /* 12px */
  --text-sm: 0.875rem;    /* 14px */
  --text-base: 1rem;      /* 16px */
  --text-lg: 1.125rem;    /* 18px */
  --text-xl: 1.25rem;     /* 20px */
  --text-2xl: 1.5rem;     /* 24px */
  --text-3xl: 1.875rem;   /* 30px */
  --text-4xl: 2.25rem;    /* 36px */
  --text-5xl: 3rem;       /* 48px */
  --text-6xl: 3.75rem;    /* 60px */
  --text-7xl: 4.5rem;     /* 72px */
}
```

## Font Weights

```css
:root {
  --weight-thin: 100;
  --weight-extralight: 200;
  --weight-light: 300;
  --weight-normal: 400;    /* Body text */
  --weight-medium: 500;    /* Emphasis */
  --weight-semibold: 600;  /* Headings */
  --weight-bold: 700;      /* Strong emphasis */
  --weight-extrabold: 800;
  --weight-black: 900;     /* Display text */
}

body {
  font-weight: var(--weight-normal);
}

h1, h2, h3 {
  font-weight: var(--weight-bold);
}

strong, b {
  font-weight: var(--weight-semibold);
}
```

## Line Height (Leading)

### Recommended Line Heights

```css
:root {
  --leading-none: 1;        /* Headings */
  --leading-tight: 1.25;    /* Large headings */
  --leading-snug: 1.375;    /* Subheadings */
  --leading-normal: 1.5;    /* Body text ⭐ */
  --leading-relaxed: 1.625; /* Long-form content */
  --leading-loose: 2;       /* Poetry, spacing */
}

body {
  line-height: var(--leading-normal);
}

h1, h2, h3 {
  line-height: var(--leading-tight);
}

.prose {
  line-height: var(--leading-relaxed);
}
```

### Rules of Thumb

- **Headings**: 1.1 - 1.3 (tighter)
- **Body text**: 1.5 - 1.6 (comfortable)
- **Long-form**: 1.6 - 1.8 (easier reading)
- **Small text**: 1.4 - 1.5 (needs more space)

## Letter Spacing (Tracking)

```css
:root {
  --tracking-tighter: -0.05em;
  --tracking-tight: -0.025em;
  --tracking-normal: 0;
  --tracking-wide: 0.025em;
  --tracking-wider: 0.05em;
  --tracking-widest: 0.1em;
}

/* Large headings - tighter */
h1 {
  letter-spacing: var(--tracking-tight);
}

/* Uppercase text - wider */
.uppercase {
  text-transform: uppercase;
  letter-spacing: var(--tracking-wider);
}

/* Small text - slightly wider */
small {
  letter-spacing: var(--tracking-wide);
}
```

## Typography Best Practices

### Hierarchy

```css
/* Clear visual hierarchy */
h1 {
  font-size: var(--text-4xl);
  font-weight: var(--weight-bold);
  line-height: var(--leading-tight);
  margin-bottom: 1rem;
}

h2 {
  font-size: var(--text-3xl);
  font-weight: var(--weight-semibold);
  line-height: var(--leading-snug);
  margin-bottom: 0.875rem;
}

p {
  font-size: var(--text-base);
  font-weight: var(--weight-normal);
  line-height: var(--leading-relaxed);
  margin-bottom: 1rem;
  color: var(--text-secondary);
}

strong {
  font-weight: var(--weight-semibold);
  color: var(--text-primary);
}
```

### Measure (Line Length)

Optimal reading width: **45-75 characters per line**

```css
.prose {
  max-width: 65ch; /* Characters */
  /* Or: max-width: 680px; */
}
```

### Responsive Typography

```css
/* Fluid typography */
h1 {
  font-size: clamp(2rem, 5vw, 4rem);
}

/* Or media queries */
h1 {
  font-size: 2rem; /* Mobile */
}

@media (min-width: 768px) {
  h1 {
    font-size: 3rem; /* Tablet */
  }
}

@media (min-width: 1024px) {
  h1 {
    font-size: 4rem; /* Desktop */
  }
}
```

## Common Typography Mistakes

❌ Too many font families (limit to 2-3)
❌ Poor contrast (text too light on background)
❌ Lines too long (over 75 characters)
❌ Line height too tight for body text
❌ Inconsistent hierarchy
❌ All caps for long paragraphs (hard to read)
❌ Mixing similar font weights (400 and 500)
❌ Center-aligning body text (hard to scan)
❌ Ignoring mobile font sizes (too small)
❌ Not using web-safe fallbacks

## Tools & Resources

### Color Tools
- **Coolors.co** - Palette generator
- **Adobe Color** - Color wheel
- **Contrast Checker** - WebAIM
- **Who Can Use** - Contrast simulator
- **Colorable** - Color contrast tester

### Typography Tools
- **Type Scale** - Modular scale generator
- **Google Fonts** - Free web fonts
- **Font Pair** - Font combination inspiration
- **Typewolf** - Typography inspiration
- **Modular Scale** - Scale calculator

## Conclusion

Color and typography are the foundation of visual design. Master these fundamentals to create beautiful, accessible, and professional interfaces. Remember: good typography is invisible - readers shouldn't notice it, they should just be able to read effortlessly!
