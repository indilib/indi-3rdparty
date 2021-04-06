from setup import ExtensionInstaller

def loader():
    return weatherradioInstaller()

class weatherradioInstaller(ExtensionInstaller):
    def __init__(self):
        super(weatherradioInstaller, self).__init__(
            version="0.1",
            name='weatherradio',
            description='weather radio driver',
            author="Mark Cheverton",
            author_email="mark.cheverton@ecafe.org",
            config={
                'StdReport': {
                    'weatherradioJSONReport': {
                        'HTML_ROOT':'/var/www/weatherradio',
                        'skin': 'weatherradioJSON',
                        'enable': True
                    }
                }
            },
            files=[('bin/user', ['bin/user/weatherradio.py',]),
                   ('skins/weatherradioJSON', ['skins/weatherradioJSON/w.tmpl',
                                               'skins/weatherradioJSON/skin.conf'])]
        )
