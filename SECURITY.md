# Security Policy

lvdExplorer has direct access to the local filesystem (and, on Windows,
invokes native shell/COM APIs), so security reports are taken seriously
even though this is a small, early-stage project.

## Reporting a Vulnerability

Please **do not** open a public issue for a security vulnerability.
Instead, email **giorgio@lvdsystems.it** with:

- A description of the issue and its potential impact.
- Steps to reproduce, if possible.
- The version/commit and platform (Windows/Linux) affected.

You should get an acknowledgment within a few days. Once a fix is ready,
credit will be given in the release notes unless you'd prefer otherwise.

## Supported Versions

This project is pre-1.0 and does not yet maintain multiple release
branches — security fixes land on `main` and the most recent release.
