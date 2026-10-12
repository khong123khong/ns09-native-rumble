# Maintainer publication workflow

Publishing is separate from building. Review the tracked file list, source changes, licenses, release notes and asset checksums before making a repository public or pushing a tag. Do not publish a private development ZIP; it contains the original personal identity.

The GitHub app connection and Windows GitHub CLI authentication are separate. A connected app does not automatically authenticate `gh`. Use the official CLI browser/device flow if CLI authentication is needed:

```text
gh auth status --hostname github.com
gh auth login --hostname github.com --git-protocol https --web
```

Complete sign-in and review requested permissions on GitHub. Do not paste a token into an issue, chat, repository file or command transcript. Confirm the signed-in account is the intended repository owner before creating `ns09-native-rumble`. [Official authentication documentation](https://cli.github.com/manual/gh_auth_login).

After approval, create the repository as Public, commit only the reviewed tracked files, push `main`, create tag `v1.0.0` and create the Release from `RELEASE-NOTES.md`. Mark the initial public release **pre-release**; identity adaptation is not device-validated yet. Upload only the reviewed module ZIP, SHA256SUMS.txt and build-manifest.json.

Build tools, SDKs, private captures/logs, temporary build files, existing private ZIPs and the local approval report must stay outside Git and Release assets. A source archive must be generated from the approved tracked manifest. Check the remote owner and repository identity before reusing an existing repository; never overwrite an unrelated project.
