# Modern Web Design Patterns Skill

## Purpose
Learn contemporary web design patterns, component libraries, and UI trends used in professional applications. This skill covers reusable patterns for common UI challenges.

## When to Use
- Building modern web applications
- Creating component libraries
- Implementing common UI patterns (modals, dropdowns, etc.)
- Following current design trends
- Improving user experience

## Component Patterns

### 1. Cards
The most versatile component in modern design

```css
/* Basic Card */
.card {
  background: white;
  border-radius: 12px;
  padding: 24px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  transition: all 0.3s;
}

.card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.15);
}

/* Card with Image */
.image-card {
  overflow: hidden;
}

.card-image {
  width: 100%;
  height: 200px;
  object-fit: cover;
  transition: transform 0.3s;
}

.image-card:hover .card-image {
  transform: scale(1.05);
}

/* Glassmorphism Card */
.glass-card {
  background: rgba(255, 255, 255, 0.8);
  backdrop-filter: blur(10px);
  border: 1px solid rgba(255, 255, 255, 0.3);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);
}

/* Neumorphic Card */
.neuro-card {
  background: #e0e5ec;
  box-shadow:
    9px 9px 16px rgba(163, 177, 198, 0.6),
    -9px -9px 16px rgba(255, 255, 255, 0.5);
  border-radius: 20px;
}
```

### 2. Buttons

```css
/* Primary Button */
.btn {
  padding: 12px 24px;
  border-radius: 8px;
  font-weight: 600;
  border: none;
  cursor: pointer;
  transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  position: relative;
  overflow: hidden;
}

.btn-primary {
  background: linear-gradient(135deg, #667eea, #764ba2);
  color: white;
}

.btn-primary::before {
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  width: 0;
  height: 0;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.3);
  transform: translate(-50%, -50%);
  transition: width 0.6s, height 0.6s;
}

.btn-primary:hover::before {
  width: 300px;
  height: 300px;
}

.btn-primary:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 24px rgba(102, 126, 234, 0.4);
}

/* Secondary Button */
.btn-secondary {
  background: white;
  color: #667eea;
  border: 2px solid #667eea;
}

.btn-secondary:hover {
  background: #667eea;
  color: white;
}

/* Ghost Button */
.btn-ghost {
  background: transparent;
  color: #667eea;
  border: 1px solid transparent;
}

.btn-ghost:hover {
  background: rgba(102, 126, 234, 0.1);
  border-color: #667eea;
}

/* Icon Button */
.btn-icon {
  width: 40px;
  height: 40px;
  padding: 0;
  border-radius: 50%;
  display: flex;
  justify-content: center;
  align-items: center;
}

/* Loading Button */
.btn-loading {
  position: relative;
  color: transparent;
  pointer-events: none;
}

.btn-loading::after {
  content: '';
  position: absolute;
  width: 16px;
  height: 16px;
  top: 50%;
  left: 50%;
  margin-left: -8px;
  margin-top: -8px;
  border: 2px solid white;
  border-radius: 50%;
  border-top-color: transparent;
  animation: spin 0.6s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
```

### 3. Navigation

```css
/* Modern Navbar */
.navbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 32px;
  background: rgba(255, 255, 255, 0.9);
  backdrop-filter: blur(10px);
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
  position: sticky;
  top: 0;
  z-index: 100;
}

.nav-links {
  display: flex;
  gap: 32px;
  list-style: none;
}

.nav-link {
  color: #1e293b;
  text-decoration: none;
  font-weight: 500;
  position: relative;
  padding: 8px 0;
  transition: color 0.3s;
}

.nav-link::after {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  width: 0;
  height: 2px;
  background: #667eea;
  transition: width 0.3s;
}

.nav-link:hover::after,
.nav-link.active::after {
  width: 100%;
}

/* Sidebar Navigation */
.sidebar-nav {
  width: 280px;
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(10px);
  height: 100vh;
  position: fixed;
  left: 0;
  top: 0;
  box-shadow: 4px 0 12px rgba(0, 0, 0, 0.1);
}

.sidebar-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 16px 24px;
  color: #1e293b;
  text-decoration: none;
  transition: all 0.3s;
  position: relative;
}

.sidebar-item::before {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 0;
  background: #667eea;
  transition: width 0.3s;
}

.sidebar-item:hover::before,
.sidebar-item.active::before {
  width: 4px;
}

.sidebar-item:hover,
.sidebar-item.active {
  background: rgba(102, 126, 234, 0.1);
  padding-left: 32px;
}
```

### 4. Modals & Overlays

```css
/* Modal Backdrop */
.modal-backdrop {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.5);
  backdrop-filter: blur(4px);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 1000;
  animation: fadeIn 0.3s;
}

@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}

/* Modal Content */
.modal {
  background: white;
  border-radius: 16px;
  padding: 32px;
  max-width: 600px;
  width: 90%;
  max-height: 80vh;
  overflow-y: auto;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
  animation: slideUp 0.3s;
  position: relative;
}

@keyframes slideUp {
  from {
    opacity: 0;
    transform: translateY(40px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

/* Modal Header */
.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
  padding-bottom: 16px;
  border-bottom: 1px solid #e2e8f0;
}

/* Close Button */
.modal-close {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  border: none;
  background: #f1f5f9;
  color: #64748b;
  cursor: pointer;
  display: flex;
  justify-content: center;
  align-items: center;
  transition: all 0.3s;
}

.modal-close:hover {
  background: #e2e8f0;
  color: #1e293b;
}
```

### 5. Dropdowns & Menus

```css
/* Dropdown Container */
.dropdown {
  position: relative;
  display: inline-block;
}

/* Dropdown Button */
.dropdown-toggle {
  padding: 10px 16px;
  background: white;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  cursor: pointer;
  display: flex;
  align-items: center;
  gap: 8px;
}

/* Dropdown Menu */
.dropdown-menu {
  position: absolute;
  top: calc(100% + 8px);
  left: 0;
  background: white;
  border-radius: 12px;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.15);
  min-width: 200px;
  padding: 8px;
  opacity: 0;
  visibility: hidden;
  transform: translateY(-10px);
  transition: all 0.3s;
  z-index: 100;
}

.dropdown.open .dropdown-menu {
  opacity: 1;
  visibility: visible;
  transform: translateY(0);
}

/* Dropdown Item */
.dropdown-item {
  padding: 12px 16px;
  border-radius: 6px;
  cursor: pointer;
  transition: background 0.2s;
  display: flex;
  align-items: center;
  gap: 12px;
}

.dropdown-item:hover {
  background: #f1f5f9;
}

/* Dropdown Divider */
.dropdown-divider {
  height: 1px;
  background: #e2e8f0;
  margin: 8px 0;
}
```

### 6. Tooltips

```css
/* Tooltip Container */
.tooltip {
  position: relative;
  display: inline-block;
}

/* Tooltip Text */
.tooltip-text {
  position: absolute;
  bottom: calc(100% + 8px);
  left: 50%;
  transform: translateX(-50%);
  background: #1e293b;
  color: white;
  padding: 8px 12px;
  border-radius: 6px;
  font-size: 14px;
  white-space: nowrap;
  opacity: 0;
  visibility: hidden;
  transition: all 0.3s;
  z-index: 100;
}

/* Tooltip Arrow */
.tooltip-text::after {
  content: '';
  position: absolute;
  top: 100%;
  left: 50%;
  transform: translateX(-50%);
  border: 5px solid transparent;
  border-top-color: #1e293b;
}

/* Show on Hover */
.tooltip:hover .tooltip-text {
  opacity: 1;
  visibility: visible;
}

/* Tooltip Positions */
.tooltip-text.bottom {
  bottom: auto;
  top: calc(100% + 8px);
}

.tooltip-text.bottom::after {
  top: auto;
  bottom: 100%;
  border-top-color: transparent;
  border-bottom-color: #1e293b;
}
```

### 7. Toast Notifications

```css
/* Toast Container */
.toast-container {
  position: fixed;
  top: 24px;
  right: 24px;
  z-index: 9999;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

/* Toast */
.toast {
  background: white;
  border-radius: 12px;
  padding: 16px 20px;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.15);
  display: flex;
  align-items: center;
  gap: 12px;
  min-width: 300px;
  animation: slideInRight 0.3s;
  position: relative;
  overflow: hidden;
}

@keyframes slideInRight {
  from {
    transform: translateX(100%);
    opacity: 0;
  }
  to {
    transform: translateX(0);
    opacity: 1;
  }
}

/* Toast Progress Bar */
.toast::before {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  height: 3px;
  background: #667eea;
  animation: progress 3s linear;
}

@keyframes progress {
  from { width: 100%; }
  to { width: 0%; }
}

/* Toast Types */
.toast.success {
  border-left: 4px solid #10b981;
}

.toast.success::before {
  background: #10b981;
}

.toast.error {
  border-left: 4px solid #ef4444;
}

.toast.error::before {
  background: #ef4444;
}

.toast.warning {
  border-left: 4px solid #f59e0b;
}

.toast.warning::before {
  background: #f59e0b;
}

.toast.info {
  border-left: 4px solid #3b82f6;
}

.toast.info::before {
  background: #3b82f6;
}
```

### 8. Loading States

```css
/* Skeleton Loader */
.skeleton {
  background: linear-gradient(
    90deg,
    #f0f0f0 25%,
    #e0e0e0 50%,
    #f0f0f0 75%
  );
  background-size: 200% 100%;
  animation: loading 1.5s infinite;
  border-radius: 4px;
}

@keyframes loading {
  0% { background-position: 200% 0; }
  100% { background-position: -200% 0; }
}

.skeleton-text {
  height: 16px;
  margin-bottom: 8px;
}

.skeleton-title {
  height: 24px;
  width: 60%;
  margin-bottom: 12px;
}

.skeleton-avatar {
  width: 50px;
  height: 50px;
  border-radius: 50%;
}

/* Spinner */
.spinner {
  width: 40px;
  height: 40px;
  border: 4px solid #f1f5f9;
  border-top-color: #667eea;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

/* Progress Bar */
.progress-bar {
  width: 100%;
  height: 8px;
  background: #f1f5f9;
  border-radius: 4px;
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #667eea, #764ba2);
  border-radius: 4px;
  transition: width 0.3s;
  animation: shimmer 2s infinite;
}

@keyframes shimmer {
  0% { transform: translateX(-100%); }
  100% { transform: translateX(100%); }
}
```

### 9. Forms & Inputs

```css
/* Input Field */
.input-group {
  margin-bottom: 20px;
}

.input-label {
  display: block;
  margin-bottom: 8px;
  font-weight: 500;
  color: #1e293b;
}

.input-field {
  width: 100%;
  padding: 12px 16px;
  border: 2px solid #e2e8f0;
  border-radius: 8px;
  font-size: 16px;
  transition: all 0.3s;
}

.input-field:focus {
  outline: none;
  border-color: #667eea;
  box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
}

.input-field.error {
  border-color: #ef4444;
}

.input-field.error:focus {
  box-shadow: 0 0 0 3px rgba(239, 68, 68, 0.1);
}

/* Input with Icon */
.input-with-icon {
  position: relative;
}

.input-icon {
  position: absolute;
  left: 16px;
  top: 50%;
  transform: translateY(-50%);
  color: #94a3b8;
}

.input-with-icon .input-field {
  padding-left: 44px;
}

/* Checkbox */
.checkbox {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
}

.checkbox input[type="checkbox"] {
  appearance: none;
  width: 20px;
  height: 20px;
  border: 2px solid #cbd5e1;
  border-radius: 4px;
  cursor: pointer;
  position: relative;
  transition: all 0.3s;
}

.checkbox input[type="checkbox"]:checked {
  background: #667eea;
  border-color: #667eea;
}

.checkbox input[type="checkbox"]:checked::after {
  content: '✓';
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  color: white;
  font-size: 14px;
}

/* Toggle Switch */
.toggle {
  position: relative;
  width: 48px;
  height: 24px;
  background: #cbd5e1;
  border-radius: 12px;
  cursor: pointer;
  transition: background 0.3s;
}

.toggle.active {
  background: #667eea;
}

.toggle-slider {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 20px;
  height: 20px;
  background: white;
  border-radius: 50%;
  transition: transform 0.3s;
}

.toggle.active .toggle-slider {
  transform: translateX(24px);
}
```

### 10. Badges & Tags

```css
/* Badge */
.badge {
  display: inline-block;
  padding: 4px 12px;
  border-radius: 12px;
  font-size: 12px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.badge.primary {
  background: rgba(102, 126, 234, 0.1);
  color: #667eea;
}

.badge.success {
  background: rgba(16, 185, 129, 0.1);
  color: #10b981;
}

.badge.danger {
  background: rgba(239, 68, 68, 0.1);
  color: #ef4444;
}

.badge.warning {
  background: rgba(245, 158, 11, 0.1);
  color: #f59e0b;
}

/* Tag with Close */
.tag {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px;
  background: #f1f5f9;
  border-radius: 6px;
  font-size: 14px;
}

.tag-close {
  width: 16px;
  height: 16px;
  border-radius: 50%;
  background: #cbd5e1;
  border: none;
  cursor: pointer;
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 10px;
  transition: background 0.2s;
}

.tag-close:hover {
  background: #94a3b8;
}
```

## Design Trends 2024-2025

### 1. Glassmorphism
Frosted glass effect with blur

### 2. Neumorphism/Soft UI
Soft, extruded appearance

### 3. Dark Mode
Essential for modern apps

### 4. 3D Elements
Depth and perspective

### 5. Micro-interactions
Small animations on user action

### 6. Asymmetric Layouts
Breaking the grid for visual interest

### 7. Bold Typography
Large, statement fonts

### 8. Gradient Overlays
Vibrant, multi-color gradients

### 9. Minimalism
Clean, focused designs

### 10. Animated Illustrations
SVG animations and Lottie

## Best Practices

✅ Keep components reusable and modular
✅ Use semantic class names (BEM methodology)
✅ Provide feedback for all interactions
✅ Ensure accessibility (keyboard, screen readers)
✅ Test across browsers and devices
✅ Optimize for performance (lazy load, code splitting)
✅ Document component usage
✅ Version control design system
✅ Maintain consistency across patterns
✅ Follow mobile-first approach

## Conclusion

Modern web patterns provide tested, proven solutions to common UI challenges. Build a component library using these patterns, customize for your brand, and create consistent, professional interfaces efficiently. Remember: good design patterns are invisible - users shouldn't notice them, they should just work!
