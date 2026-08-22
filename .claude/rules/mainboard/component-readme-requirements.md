---
paths:
  - "firmware/mainboard/components/**"
---

# Component README Requirements

Applies when working under `firmware/mainboard/components/`.

- Every folder under `components/` should include a README.md file.
- Each component README.md should include a Purpose section describing what the component does.
- Each component README.md should include an SDKConfig Requirements section listing the sdkconfig options needed to run the component.
- When several components are missing READMEs, prefer a one-time sweep to add them all rather than doing it piecemeal.

See also `firmware/mainboard/AGENTS.md` ("Prefer component-local README files when editing a component under `components/`").
