import requests
import hashlib
import json

def sha256_hex(data: str) -> str:
    return hashlib.sha256(data.encode('utf-8')).hexdigest()

def main():
    # Initial setup
    username = "test"
    user_password = "test"
    base_url = "http://127.0.0.1:8869"

    # Step 1: Send initial POST to /api/auth
    auth_url = f"{base_url}/api/auth"
    payload = {"username": username}

    try:
        response = requests.post(auth_url, json=payload)
        response.raise_for_status()
        data = response.json()
        print("Initial authentication response:")
        print(json.dumps(data, indent=4))
    except requests.exceptions.RequestException as e:
        print(f"HTTP request failed: {e}")
        return
    except json.JSONDecodeError:
        print("Failed to decode response as JSON.")
        return

    # Step 2: Extract required fields
    sessionuuid = data.get("sessionuuid")
    sessionkey = data.get("sessionkey")
    loginsalt = data.get("loginsalt")

    if not all([sessionuuid, sessionkey, loginsalt]):
        print("Missing one or more required fields in response.")
        return

    # Step 3: Compute password hash
    salted_password = sha256_hex(loginsalt + user_password)
    clientkey_hash = sha256_hex(sessionkey + salted_password)
    sessionkey_hash = sha256_hex(sessionkey + clientkey_hash)

    print("\nComputed session key hash:")
    print(f"sessionkey_hash: {sessionkey_hash}")

    # Step 4: POST sessionkey_hash to /api/auth/{sessionuuid}
    login_url = f"{base_url}/api/auth/{sessionuuid}"
    login_payload = {"clientkey": sessionkey, "password": sessionkey_hash}

    try:
        login_response = requests.post(login_url, json=login_payload)
        login_response.raise_for_status()
        login_data = login_response.json()
        print("\nFinal login response:")
        print(json.dumps(login_data, indent=4))
    except requests.exceptions.RequestException as e:
        print(f"Final login request failed: {e}")
    except json.JSONDecodeError:
        print("Failed to decode final login response as JSON.")
        
    token = login_data.get("token")

    if token:
        print("\nExtracted token:")
        print(token)
    else:
        print("No token found in response.")
        
        
    # Step 5: Send GET to /api/build/ with Bearer token
    if token:
        build_url = f"{base_url}/api/build/?archived=1"
        headers = {
            "Authorization": f"Bearer {token}"
        }

        try:
            build_response = requests.get(build_url, headers=headers)
            build_response.raise_for_status()
            build_data = build_response.json()
            print("\nBuild data response:")
            print(json.dumps(build_data, indent=4))
        except requests.exceptions.RequestException as e:
            print(f"GET /api/build/ failed: {e}")
        except json.JSONDecodeError:
            print("Failed to decode build response as JSON.")


if __name__ == "__main__":
    main()
