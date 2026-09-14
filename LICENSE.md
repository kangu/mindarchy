# Mindarchy license

Copyright 2026 Mindarchy contributors.

Original Mindarchy source code, documentation, build scripts, and original assets
in this repository are licensed under the **Apache License, Version 2.0**
(SPDX identifier: `Apache-2.0`), except where a file or component specifies
different terms. The grant covers only rights that the respective contributors
are entitled to license. Contributors retain ownership of their contributions.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this material except in compliance with the License. You may obtain a copy at
[the Apache Software Foundation](https://www.apache.org/licenses/LICENSE-2.0).
Unless required by applicable law or agreed to in writing, this material is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
either express or implied. See the License for the specific language governing
permissions and limitations under the License.

The complete, unmodified Apache 2.0 text appears at the end of this file.
The explanations and release checklist below do not amend that license or impose
additional restrictions on recipients. They describe separate dependency
obligations and the project's recommended release practices. The applicable
license texts control if a summary differs from them.

## 1. Scope and permissions

Apache 2.0 permits personal and commercial use, modification, redistribution,
self-hosting, paid hosting, and incorporation into other software, including
proprietary products, subject to its conditions. It does not require payment to
Mindarchy or publication of changes merely because someone operates a hosted
service or distributes an Apache-licensed derivative. Dependencies can impose
their own source-disclosure requirements.

When redistributing Apache-licensed material, include the license, retain
applicable notices in distributed source, and prominently identify modified
files. If a distribution contains a `NOTICE` file, preserve its applicable
attributions as specified by section 4. Apache 2.0 does not require creating an
otherwise unnecessary `NOTICE` file.

The license includes a contributor patent grant limited to the claims described
in section 3, with termination provisions for certain patent litigation. It is
not a guarantee that no third party holds relevant patents. Warranty and
liability provisions are in sections 7–9.

This file applies to this Mindarchy application repository. It does not
automatically license sibling repositories, operating-system software, external
services, or content imported by users. Mind maps, attachments, and ordinary
exports do not acquire the application's license merely by being created or
opened with Mindarchy. Their existing rights and licenses continue to apply.

## 2. Third-party components keep their licenses

The Apache license above does not relicense Qt, imported icons, runtime
libraries, fonts, or other third-party material. Preserve component-specific
copyright notices, license texts, exceptions, and attribution requirements.

The following is an initial inventory based on the application sources and
release scripts reviewed on **2026-09-11**, not a complete inventory of every
binary, plugin, or asset in a release:

| Component | Licensing treatment | Evidence and release considerations |
| --- | --- | --- |
| Original Mindarchy material | Apache-2.0, unless separately marked | This file; preserve any more specific notices. |
| Qt Core, Gui, Network, QML, Quick, Quick Controls | Use the LGPLv3 option available for these modules in the selected Qt release | Direct dependencies in [CMakeLists.txt](CMakeLists.txt). Check the exact Qt version and file-level exceptions. |
| Qt Test | Check its applicable open-source terms | Used by the test targets. Do not assume test tools are shipped in the application. |
| Additional Qt libraries and plugins | Determine from the actual deployed files | QML imports, platform plugins, image codecs, styles, and transitive libraries can add components beyond the direct CMake list. |
| Lucide toolbar icons and included Feather-derived icons | ISC and, for the identified Feather-derived material, MIT | Preserve the complete [icon license](qml/icons/LICENSE.txt) and [source attribution](qml/icons/README.md). Embedding SVGs as resources does not remove their notice requirements. |
| Third-party code within Qt | Each component's original terms | Use the selected Qt build's notices and SBOM; do not label all bundled code LGPL. |
| Microsoft Visual C++ runtime, when redistributed | Microsoft's applicable redistribution terms | The Windows release script deploys runtime DLLs. They are not licensed by this file. |
| Operating-system frameworks and installed fonts | Their respective platform or font terms | Using an installed framework or font does not grant permission to redistribute its files. Separately review any fonts or other assets added to a package. |

The distribution should include a readable component inventory with exact
versions, selected licenses, applicable copyright notices, and source locations.
Qt's [third-party code documentation](https://doc.qt.io/qt-6/licenses-used-in-qt.html)
and [SBOM documentation](https://doc.qt.io/qt-6/sbom.html) are starting points.
An SBOM assists the review; it does not replace the licenses or source delivery.

## 3. Qt under LGPLv3

Mindarchy's intended open-source Qt distribution model is **Apache-licensed
application code using dynamically linked LGPLv3 Qt libraries**. That combination
does not require the original application code to be relicensed as LGPL or GPL
solely because it uses those libraries.

Commercial activity does not by itself require a commercial Qt subscription.
The condition is compliance with the applicable open-source terms for every
component used or distributed. A commercial Qt agreement is an alternative when
needed; this document does not grant one or override an existing agreement.

See [Qt licensing](https://doc.qt.io/qt-6/licensing.html),
[Qt's LGPL obligations guidance](https://www.qt.io/development/open-source-lgpl-obligations),
and the actual [LGPLv3](https://www.gnu.org/licenses/lgpl-3.0.txt), which incorporates
[GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt).

### 3.1 Notices and license copies

For an LGPL Qt distribution:

- Give prominent notice that the application uses Qt and that the relevant Qt
  libraries and their use are covered by LGPLv3.
- Supply the complete LGPLv3 and GPLv3 texts, together with applicable Qt and
  third-party notices. A web link alone is not a bundled copy of a license.
- If the application displays copyright notices during execution, include the
  appropriate Qt notice and a reference to the supplied licenses as required by
  LGPLv3 section 4(c).
- An accessible About / Third-party licenses entry and locally installed license
  files are the recommended presentation. Keep the files available after
  installation and when offline.

Do not label an installer containing Qt and other components as though every
byte were exclusively Apache-licensed. Describe Mindarchy's original code as
Apache-licensed and identify the separate dependency licenses.

### 3.2 Exact corresponding source

When distributing Qt object code, satisfy the source-conveyance requirements
applicable to that distribution, including GPLv3 section 6 as incorporated by
LGPLv3. Unmodified Qt binaries still have corresponding-source obligations.

For downloadable Mindarchy releases, the recommended approach is to publish
the matching Qt source and any required patches and build material alongside
the binary download, with access that satisfies the chosen license mechanism:

- Identify the precise Qt version, relevant repositories or source archives,
  build configuration, and any distributor patches. A version number alone is
  insufficient if the binaries contain downstream changes.
- Include the source needed for the Qt components actually shipped, including
  relevant generated-code inputs, interface definitions, and build/install
  scripts within the license's definition of Corresponding Source.
- Record checksums for the archives and connect them to a specific release.
- Place clear source-download directions next to the binary download, and keep
  them available for the duration required by the selected distribution method.
  A separate source server can be used when the applicable conditions are met.
- Do not rely solely on a general upstream download homepage, a moving Git
  branch, or the availability of Mindarchy's own source.

A written source offer is another option only when appropriate under the
applicable GPLv3 section 6 distribution method. It creates specific fulfillment
and duration obligations; do not substitute an informal "source on request"
statement for a compliant offer.

No hosted source archive, working source-offer service, or release-specific
source fulfillment is established merely by adding this document.

### 3.3 Replacement, relinking, and installation

LGPLv3 section 4 provides a shared-library route and a route involving
Corresponding Application Code suitable for recombination or relinking.
Dynamic linking is the preferred release practice because it makes replacing a
compatible Qt library more straightforward.

Verify that a user can actually install and run a modified build using an
interface-compatible Qt library. File layout, library lookup, plugin versions,
signatures, sandboxing, and installer permissions can affect that ability.

Distribution terms must not prohibit the modification of the covered library
or reverse engineering for debugging those modifications. Provide Installation
Information where LGPLv3 section 4(e), through GPLv3 section 6, requires it.

Static linking is not automatically forbidden and does not automatically
require relicensing original Mindarchy source. It does require selecting and
fulfilling the appropriate LGPL route, including sufficient application code,
build materials, and permissions for relinking. Publishing source is helpful
but is not by itself evidence that this route works.

### 3.4 Changes to Qt

If a release modifies Qt, preserve the applicable Qt license and notices,
identify the changes, and include their corresponding source through the
selected source-delivery mechanism. Keep patches and build instructions tied
to the released binaries.

Do not put Qt modifications under an Apache-only notice. Code copied from Qt
into the application also needs review under its original terms; changing its
location does not change its license.

### 3.5 GPL-only modules and development tools

Review new Qt dependencies before adopting them. Some modules offer GPL or
commercial licensing without an LGPL option. Examples in the reviewed Qt
licensing documentation include Qt Quick 3D, Qt Graphs, Qt Virtual Keyboard,
and Qt Wayland Compositor. The list can change; check the exact release.

Linking a GPL-only library may require distributing the combined application
under GPL-compatible terms. Options include choosing an LGPL-compatible
alternative, obtaining an appropriate commercial license, or deliberately
adopting GPL-compliant distribution for the combined work.

Apache 2.0 is compatible with GPLv3 in the direction described by the
[Apache Software Foundation](https://www.apache.org/licenses/GPL-compatibility.html).
Original Apache notices can remain, but GPL-covered code cannot simply be
redistributed as Apache-only. Do not assume compatibility with GPLv2-only code.

A GPL-licensed build tool is a different question from a GPL-linked runtime
library. Check the tool's exceptions and any material it places in the output;
do not infer an application's license from the tool license alone.

Using Qt's Wayland client support on Omarchy is not the same as using the
Qt Wayland Compositor module.

## 4. Platform-specific release handling

These are release practices to implement and verify, not claims that existing
installers have passed a complete compliance review.

### macOS

- Inventory the main application, Qt frameworks, plugins, and the Quick Look
  preview extension, including dependencies copied into nested bundles.
- Include this license, component notices, and complete dependency license texts
  in a locally accessible location such as `Contents/Resources/licenses/`.
- Provide matching Qt source and the release's build configuration.
- Test the user's route for replacing compatible Qt frameworks or rebuilding,
  locally re-signing, installing, and running the modified application.
  Include any necessary instructions for nested components.
- Check how the shipped hardened-runtime settings, library validation, and
  extension sandbox affect that route. Do not assume successful notarization
  also verifies LGPL compliance.
- Keep private release-signing credentials private. The intended user rebuild
  route should use the user's own local signing identity where supported.
  If the shipped restrictions prevent running modified builds, resolve the
  distribution design or licensing before release.
- Review Mac App Store distribution separately before adopting it. The
  `macdeployqt -appstore-compliant` option is not a legal compliance certificate.

### Windows

- Inventory Qt DLLs, platform/image/QML plugins, and runtime DLLs after deployment
  and before building the Inno Setup installer.
- Install the license texts and attributions with the application; verify that
  the installer actually includes them and that standard users can read them.
- Document a supported way to use compatible replacement Qt DLLs, such as a
  writable application copy or a rebuilt installation. Test that it launches.
- Check the Microsoft runtime and installer components under their own terms.
- Provide release-specific Qt source directions and build instructions.

### Omarchy and other Linux distributions

- Prefer the distribution's shared Qt packages when practical. Record their
  versions and relevant downstream patches for reproducibility.
- When Mindarchy does not redistribute the Qt binaries, their package
  distributor handles that conveyance. Mindarchy still needs its applicable
  combined-work notices and license obligations; system-package dependencies
  are not a blanket exemption.
- Include Mindarchy's license and relevant attributions in the package's normal
  documentation/license locations and set accurate package license metadata.
- If a future AppImage, archive, container, or other release bundles Qt, apply
  the Qt binary redistribution and corresponding-source requirements to those
  bundled copies.
- Verify that the application works with compatible rebuilt distribution
  libraries or a documented alternative library installation.

## 5. Hosting, branding, and contributions

Paid hosting, storage, synchronization, compute, and support are compatible with
Apache 2.0. Users and other providers may also self-host or operate competing
services under the license. It creates no obligation to purchase Mindarchy's
hosting or share service revenue.

LGPLv3 does not have AGPL's network-use source-offer provision. Operating an
unrelated backend does not bring it under Qt's license merely because Qt clients
connect to it. Assess the backend's own dependencies and any binaries, containers,
or SDKs delivered to customers separately.

Apache 2.0 section 6 does not grant general trademark rights. Mindarchy names
and logos used as source identifiers, and Qt's trademarks, are distinct from
the copyright permissions for software or artwork. This document does not
create an additional trademark policy or claim an endorsement by Qt, Apache,
or another project.

Contributions intentionally submitted for inclusion are handled by Apache 2.0
section 5 unless explicitly stated otherwise or governed by a separate
agreement. Contributors retain their copyrights; this file does not require
assignment or create a separate contributor license agreement. Maintainers
should check provenance and preserve original notices when accepting code,
fonts, images, examples, or other assets.

## 6. Release checklist and current implementation status

Complete this checklist for each public release and retain the evidence with
that release. The checklist is maintainer guidance, not additional conditions
on the Apache license.

- [ ] Confirm the original material can be licensed under Apache 2.0 and preserve
  all component-specific exceptions.
- [ ] Record the application revision, exact Qt build, architectures, module
  inventory, plugins, and other redistributed dependencies.
- [ ] Check the selected license of every new module and any embedded/generated
  third-party material, including assets and fonts.
- [ ] Include this file, the complete LGPLv3 and GPLv3 texts, the complete icon
  license, and other applicable license/NOTICE material in every installer.
- [ ] Provide a prominent Qt notice and locally accessible license information.
- [ ] Publish exact corresponding Qt source, patches, required build material,
  and release-specific directions using a compliant distribution method.
- [ ] Test source-download access and preserve the source fulfillment mechanism.
- [ ] Verify replacement/relinking and execution of a modified build on each
  shipped platform; provide required Installation Information.
- [ ] Check installer terms, signatures, runtime restrictions, app-store rules,
  and redistributed platform components for the chosen release channel.
- [ ] Inspect installed artifacts and retain the license inventory, checksums,
  source locations, and verification results.
- [ ] Obtain appropriate legal review before the first public release and when
  changing the licensing or distribution model.

Observed implementation status at the review date:

| Area | Existing evidence | Remaining release work |
| --- | --- | --- |
| Application dependencies | CMake directly selects the Qt modules listed above; the inspected macOS executable links shared Qt 6.11.2 frameworks. | Check the complete deployed dependency tree for each release and platform. |
| Windows | [release-windows.ps1](scripts/release-windows.ps1) copies Qt license files and [THIRD-PARTY.txt](packaging/windows/THIRD-PARTY.txt); the notice describes replacing Qt DLLs. | Verify the copied license set and installed notices, include this file and icon licenses, and supply exact corresponding-source access. The existing generic upstream URL alone is not evidence of fulfillment. |
| macOS | [release-macos.py](scripts/release-macos.py) deploys frameworks/plugins and signs the app and preview extension. | Add explicit release-license/source handling and verify user rebuild/re-sign/replacement instructions. The reviewed script has no explicit step copying this document and the full required notice set. |
| Omarchy | [build_omarchy.sh](scripts/build_omarchy.sh) depends on `qt6-base`, `qt6-declarative`, and `qt6-wayland`. | Add accurate license metadata and explicitly include license/notice files in the staged source and installed package; verify the actual output. |
| Documentation | This file records the Apache grant and dependency guidance. | Adding this file does not update packaging, implement a licenses UI, publish source archives, or establish that an existing binary complies. |

This review is limited to the inspected repository and scripts. It is not a
legal opinion or certification of a completed dependency audit.

## 7. Authoritative references

- [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0.txt)
- [GNU LGPL version 3](https://www.gnu.org/licenses/lgpl-3.0.txt)
- [GNU GPL version 3](https://www.gnu.org/licenses/gpl-3.0.txt)
- [Qt licensing and module exceptions](https://doc.qt.io/qt-6/licensing.html)
- [Qt open-source obligations guidance](https://www.qt.io/development/open-source-lgpl-obligations)
- [Qt Quick Controls licensing and attributions](https://doc.qt.io/qt-6/qtquickcontrols-index.html#license-and-attributions)
- [Third-party code used in Qt](https://doc.qt.io/qt-6/licenses-used-in-qt.html)
- [Qt Software Bill of Materials](https://doc.qt.io/qt-6/sbom.html)
- [Apache 2.0 and GPL compatibility](https://www.apache.org/licenses/GPL-compatibility.html)

## 8. Full Apache License, Version 2.0

The following is the unmodified license text published by the Apache Software
Foundation, including its standard appendix. The appendix's example placeholders
are part of that official text; the project-specific notice is at the beginning
of this file.

```text

                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright [yyyy] [name of copyright owner]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```

