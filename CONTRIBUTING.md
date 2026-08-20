# Contribution Guidelines

fooyin is a free and open-source project. Contributions through code, bug reports, documentation, testing, translation, and user support are welcome.
By participating, you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Before You Start

- Search the existing issues and [CHANGELOG.md](CHANGELOG.md) before starting work.
- Open an issue or discussion before making a significant change so its scope and approach can be agreed upon first.
- Read [BUILD.md](BUILD.md) for dependencies, build options, and platform-specific instructions.

## Making Changes

- Keep changes focused and relevant.
- Avoid mixing unrelated changes in a single pull request.
- Follow the style and conventions of the surrounding code.
- Add or update tests when changing behaviour.
- Update documentation and the changelog when appropriate.

### Building and testing

The `debug` or `debug-clang` CMake presets enable tests and provide the recommended development build:

```
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

### Formatting

All code **must be formatted using Clang-format** with the repository's `.clang-format` configuration:

```
clang-format -i <files>
```

Do not submit code that conflicts with the formatting rules.

## Pull Requests

- Explain what changed and why.
- Link any related issues or discussions.
- Ensure the project builds successfully and all relevant tests pass.
- Keep follow-up commits focused on review feedback.

All changes are reviewed before merging. Review will consider technical correctness, maintainability, test coverage, and consistency with the rest of the project. 
A submission may be declined even if it works as described.

## Commit Messages

- Keep commits small and logically grouped.
- Prefix commits with a component tag in square brackets, such as `[core]`, `[gui]`, or `[plugin]`.
- Keep the first line concise, with no trailing period.
- Add a detailed description after a blank line for non-trivial changes.

Format:

```
[tag] Short summary

Optional detailed explanation of what changed and why.
```

Example:

```
[core] Fix playback state desync

Resolve an issue where pausing during buffer underrun causes incorrect state transitions.

Fixes #123
```

## AI Policy

We do not accept contributions for which generative AI was used at any stage of the development process. Please do not contribute code, documentation, tests, images, or other project material created or modified with large language models, image diffusion models, or similar tools.

This policy applies to contributions to this repository and other repositories under the [fooyin](https://github.com/fooyin) organisation. It does not apply to independently developed third-party plugins.

Once a contribution is merged, its long-term maintenance becomes the responsibility of the maintainer and other core contributors, which is why contributors must understand and take ownership of every part of their submission.

We rely on contributors to follow this policy in good faith. We may ask whether generative AI tools were used when reviewing a contribution, and pull requests that involve their use will not be accepted.

## Issues and Other Contributions

- Use the issue templates and provide clear reproduction steps for bugs.
- Include relevant logs, screenshots, and system details.
- Prefer to submit translations through [Hosted Weblate](https://hosted.weblate.org/projects/fooyin/) rather than a pull request.

## Security

- Never include credentials, secrets, or sensitive data in commits or issues.
- Report security concerns privately to the maintainer.

## Licensing

By contributing, you agree that your contributions will be licensed under the same license as fooyin.
