"""
  Copyright (c) 2024, The OpenThread Authors.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the copyright holder nor the
     names of its contributors may be used to endorse or promote products
     derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
"""

from ble.ble_connection_constants import BBTC_SERVICE_UUID, BBTC_TX_CHAR_UUID, \
    BBTC_RX_CHAR_UUID, SERVER_COMMON_NAME
from ble.ble_stream import BleStream
from ble.ble_stream_secure import BleStreamSecure
from ble import ble_scanner
from tlv.tlv import TLV
from tlv.tcat_tlv import TcatTLVType
from cli.command import Command, CommandResultNone, CommandResultTLV
from dataset.dataset import ThreadDataset
from utils import select_device_by_user_input, get_int_in_range
from os import path


class HelpCommand(Command):

    def get_help_string(self) -> str:
        return 'Display help and return.'

    async def execute_default(self, args, context):
        commands = context['commands']
        for name, command in commands.items():
            print(f'{name}')
            command.print_help(indent=1)
        return CommandResultNone()


class HelloCommand(Command):

    def get_help_string(self) -> str:
        return 'Send round trip "Hello world!" message.'

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']
        print('Sending hello world...')
        data = TLV(TcatTLVType.APPLICATION.value, bytes('Hello world!', 'ascii')).to_bytes()
        response = await bless.send_with_resp(data)
        if not response:
            return
        tlv_response = TLV.from_bytes(response)
        return CommandResultTLV(tlv_response)


class CommissionCommand(Command):

    def get_help_string(self) -> str:
        return 'Update the connected device with current dataset.'

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']
        dataset: ThreadDataset = context['dataset']

        print('Commissioning...')
        dataset_bytes = dataset.to_bytes()
        data = TLV(TcatTLVType.ACTIVE_DATASET.value, dataset_bytes).to_bytes()
        response = await bless.send_with_resp(data)
        if not response:
            return
        tlv_response = TLV.from_bytes(response)
        return CommandResultTLV(tlv_response)


class ThreadStartCommand(Command):

    def get_help_string(self) -> str:
        return 'Enable thread interface.'

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']

        print('Enabling Thread...')
        data = TLV(TcatTLVType.THREAD_START.value, bytes()).to_bytes()
        response = await bless.send_with_resp(data)
        if not response:
            return
        tlv_response = TLV.from_bytes(response)
        return CommandResultTLV(tlv_response)


class ThreadStopCommand(Command):

    def get_help_string(self) -> str:
        return 'Disable thread interface.'

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']
        print('Disabling Thread...')
        data = TLV(TcatTLVType.THREAD_STOP.value, bytes()).to_bytes()
        response = await bless.send_with_resp(data)
        if not response:
            return
        tlv_response = TLV.from_bytes(response)
        return CommandResultTLV(tlv_response)


class ThreadStateCommand(Command):

    def __init__(self):
        self._subcommands = {'start': ThreadStartCommand(), 'stop': ThreadStopCommand()}

    def get_help_string(self) -> str:
        return 'Manipulate state of the Thread interface of the connected device.'

    async def execute_default(self, args, context):
        print('Invalid usage. Provide a subcommand.')
        return CommandResultNone()


class ScanCommand(Command):

    def get_help_string(self) -> str:
        return 'Perform scan for TCAT devices.'

    async def execute_default(self, args, context):
        if not (context['ble_sstream'] is None):
            del context['ble_sstream']

        tcat_devices = await ble_scanner.scan_tcat_devices()
        device = select_device_by_user_input(tcat_devices)

        if device is None:
            return CommandResultNone()

        ble_sstream = None

        print(f'Connecting to {device}')
        ble_stream = await BleStream.create(device.address, BBTC_SERVICE_UUID, BBTC_TX_CHAR_UUID, BBTC_RX_CHAR_UUID)
        ble_sstream = BleStreamSecure(ble_stream)
        ble_sstream.load_cert(
            certfile=path.join('auth', 'commissioner_cert.pem'),
            keyfile=path.join('auth', 'commissioner_key.pem'),
            cafile=path.join('auth', 'ca_cert.pem'),
        )

        print('Setting up secure channel...')
        await ble_sstream.do_handshake(hostname=SERVER_COMMON_NAME)
        print('Done')
        context['ble_sstream'] = ble_sstream


class TbrWiFiScanSubCmd(Command):

    def get_help_string(self) -> str:
        return 'Find available Wi-Fi access points and connect to one of them.'

    def user_confirmation(self):
        while True:
            try:
                userAnswer = str(input('Do you want to continue? [Y]es/[n]o\n> '))
                if userAnswer.lower() == 'y' or userAnswer.lower() == 'yes':
                    return True
                elif userAnswer.lower() == 'n' or userAnswer.lower() == 'no':
                    return False
                else:
                    print('\nTry again.')
            except KeyboardInterrupt:
                print('\nInterrupted by user.')
                return None

    def get_password(self, networkName):
        while True:
            try:
                password = str(input(f'\nPassword for \"{networkName}\"\n> '))
                if len(password) > 0:
                    return password
                else:
                    print('\nThe password is too short. Try again.')
            except KeyboardInterrupt:
                print('\nInterrupted by user.')
                return None

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']

        print('Wi-Fi scanning...\n')
        cmd = TLV(TcatTLVType.APPLICATION.value, bytes('wifi_scan', 'ascii')).to_bytes()

        response = await bless.send_with_resp(cmd)
        if not response:
            return

        tlv_response = TLV.from_bytes(response)
        if tlv_response.value.decode("ascii").find("RESP_NOT_SUPP") != -1:
            print('Command not supported\n')
            return
        elif tlv_response.value.decode("ascii").find("RESP_FAIL") != -1:
            print('Wi-Fi status get fail\n')
            return

        wifiNetworks = []
        wifiNetworksDictKeys = ["id", "ssid", "chan", "band", "rssi", "security"]

        while True:
            response = await bless.recv(buffersize=4096, timeout=15)
            if not response:
                return
            for _resp in response:
                tlv_response = TLV.from_bytes(_resp)
                if tlv_response.value.decode("ascii").find("RESP_FAIL") != -1:
                    print('Wi-Fi status get fail\n')
                    break
                elif tlv_response.value.decode("ascii").find("RESP_OK") != -1:
                    break
                wifiNetworks.append(dict(zip(wifiNetworksDictKeys, tlv_response.value.decode("ascii").split('|'))))
            break

        if len(wifiNetworks) > 0:
            print('Found Wi-Fi networks:\n')
            print(f"{'Num' :<4}| {'SSID' :<30}| {'Chan' :<5}| {'Band' :<7}| {'RSSI' :<5}")
            for _net in wifiNetworks:
                print(f"{_net['id'] :<4}| {_net['ssid'] :<30}| {_net['chan'] :<5}| {_net['band'] :<7}| {_net['rssi'] :<5}")
        else:
            print('\nNo Wi-Fi networks found.')
            return None

        print("""\nConnect to Wi-Fi network?\n"""
              """After you continue, the network is saved as default """
              """and TBR automatically connects to this network.""")
        conf = self.user_confirmation()
        if not conf:
            return

        print('\nSelect Wi-Fi network number to connect to it')
        selected = get_int_in_range(1, len(wifiNetworks))
        ssid = wifiNetworks[selected - 1]['ssid']
        sec = wifiNetworks[selected - 1]['security']

        pwd = self.get_password(ssid)
        if pwd is None:
            return

        print('Store Wi-Fi network...\n')
        cmd = TLV(TcatTLVType.APPLICATION.value, bytes('wifi_add ' + ssid + ' ' + pwd + ' ' + sec, 'ascii')).to_bytes()
        response = await bless.send_with_resp(cmd)

        if not response:
            return

        tlv_response = TLV.from_bytes(response)
        if tlv_response.value.decode("ascii").find("RESP_FAIL") != -1:
            print('\nConnection fail\n')
        print('Wi-Fi network is saved')
        return


class TbrWiFiStatusSubCmd(Command):

    def get_help_string(self) -> str:
        return 'Get Wi-Fi connection status for TBR.'

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']

        print('Wi-Fi status get...\n')
        command = TLV(TcatTLVType.APPLICATION.value, bytes('wifi_status', 'ascii')).to_bytes()
        await bless.send(command)

        wifiStatus = {}
        wifiStatusDictKeys = ['state', 'ssid', 'rssi']

        while True:
            response = await bless.recv(buffersize=4096, timeout=15)
            if not response:
                return
            for _resp in response:
                tlv_response = TLV.from_bytes(_resp)
                if tlv_response.value.decode("ascii").find("RESP_NOT_SUPP") != -1:
                    print('Command not supported\n')
                    break
                elif tlv_response.value.decode("ascii").find("RESP_FAIL") != -1:
                    print('Wi-Fi status get fail\n')
                    break
                elif tlv_response.value.decode("ascii").find("RESP_OK") != -1:
                    break
                wifiStatus = dict(zip(wifiStatusDictKeys, tlv_response.value.decode("ascii").split('|')))
            break

        if 'state' in wifiStatus:
            if wifiStatus['state'] == 'COMPLETED':
                print(f'\tWi-Fi connected to: \"{wifiStatus["ssid"]}\" [RSSI: {wifiStatus["rssi"]}]')
            else:
                print(f'\tWi-Fi {wifiStatus["state"].lower()}')

        return


class TbrWiFiCommand(Command):

    def __init__(self):
        self._subcommands = {'scan': TbrWiFiScanSubCmd(), 'status': TbrWiFiStatusSubCmd()}

    def get_help_string(self) -> str:
        return 'Manage Wi-Fi network connection for TBR.'

    async def execute_default(self, args, context):
        print('Invalid usage. Provide a subcommand.')
        return CommandResultNone()


class TbrRebootSubCmd(Command):

    def get_help_string(self) -> str:
        return 'Reboot the TBR.'

    async def execute_default(self, args, context):
        bless: BleStreamSecure = context['ble_sstream']

        cmd = TLV(TcatTLVType.APPLICATION.value, bytes('reboot', 'ascii')).to_bytes()

        response = await bless.send_with_resp(cmd)
        if not response:
            return

        tlv_response = TLV.from_bytes(response)
        if tlv_response.value.decode("ascii").find("RESP_FAIL") != -1:
            print('Command execution fail\n')

        return CommandResultTLV(tlv_response)


class TbrCommand(Command):

    def __init__(self):
        self._subcommands = {'reboot': TbrRebootSubCmd()}

    def get_help_string(self) -> str:
        return 'Manage TBR.'

    async def execute_default(self, args, context):
        print('Invalid usage. Provide a subcommand.')
        return CommandResultNone()
