# Security Policy

## Supported Versions

As Axiom is currently in active pre-alpha development, security patches are only applied to the latest `main` branch.

| Version | Supported          | Notes                         |
| ------- | ------------------ | ----------------------------- |
| `main`  | :white_check_mark: | Latest development branch     |
| < 0.1.0 | :x:                | Pre-release development snapshots |

---

## Reporting a Vulnerability

**Please do NOT report security vulnerabilities via public GitHub issues.**

Publicly disclosing a vulnerability can put users at risk before a fix is available. Instead, please report vulnerabilities through one of the following channels:

### 1. GitHub Private Vulnerability Reporting (Recommended)
1. Navigate to the **Security** tab of this repository.
2. Click on **Advisories** on the left sidebar.
3. Click **Report a vulnerability** to open a confidential report directly to the maintainers.

### 2. Direct Contact
If you prefer not to use GitHub advisories, please contact the lead maintainer directly:
* **Contact**: emukei007@gmail.com

---

## Focus Areas & Scope

Given that Axiom is a systems language with a native JIT compiler and linear memory layout, we take the following issues particularly seriously:

* **JIT Memory Protection Bypass**: Breaches in $W\oplus X$ (Write XOR Execute) memory boundaries or unsafe executable page permissions.
* **Instruction / Cache Invalidation Flaws**: CPU cache coherency issues leading to arbitrary or malicious code execution.
* **Buffer Overflow & Memory Corruption**: Unbounded writes in the compiler driver, IR builder, or runtime arena.

---

## Response Process & Disclosure Policy

When a security report is received:

1. **Acknowledgment**: We will acknowledge receipt of your vulnerability report within **48 hours**.
2. **Investigation & Fix**: We will investigate and develop a patch in a private security fork.
3. **Coordinated Disclosure**: Once verified and merged into `main`, a public security advisory will be published, crediting the reporter (unless anonymity is requested).
