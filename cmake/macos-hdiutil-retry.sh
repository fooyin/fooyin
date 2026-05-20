#!/bin/bash

set -u

command="${1:-}"

if [[ "$command" != "create" && "$command" != "detach" ]]; then
  exec /usr/bin/hdiutil "$@"
fi

attempt=1
max_attempts="${FOOYIN_HDIUTIL_ATTEMPTS:-15}"
delay_seconds="${FOOYIN_HDIUTIL_RETRY_DELAY:-2}"

while true; do
  /usr/bin/hdiutil "$@"
  result=$?

  if [[ $result -eq 0 ]]; then
    exit 0
  fi

  if (( attempt >= max_attempts )); then
    exit "$result"
  fi

  echo "hdiutil ${command} failed with exit code ${result}; retrying $((attempt + 1))/${max_attempts}" >&2
  sleep "$delay_seconds"
  ((attempt++))
done
