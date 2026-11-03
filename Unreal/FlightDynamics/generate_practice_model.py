"""Generate the original OneClickPilot C172P practice model for JSBSim."""
# SPDX-License-Identifier: MIT
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def element(parent, tag, value=None, **attrs):
    node = ET.SubElement(parent, tag, attrs)
    if value is not None:
        node.text = str(value)
    return node


def values(parent, fields):
    for key, value in fields.items():
        element(parent, key, value)


def location(parent, name, xyz):
    node = element(parent, 'location', name=name, unit='IN')
    values(node, dict(zip(('x', 'y', 'z'), xyz)))


def table(parent, independent, rows, name=None):
    node = element(parent, 'table', **({'name': name, 'type': 'internal'} if name else {}))
    if name is None:
        element(node, 'independentVar', independent)
    element(node, 'tableData', '\n' + '\n'.join(f'{x} {y}' for x, y in rows) + '\n')
    return node


def write(root, relative, tree):
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(tree)
    ET.ElementTree(tree).write(target, encoding='utf-8', xml_declaration=True)


def generate(root):
    aircraft = ET.Element('fdm_config', name='OneClickPilot C172P practice model', version='2.0', release='ALPHA')
    header = element(aircraft, 'fileheader')
    element(header, 'author', 'OneClickPilot contributors')
    element(header, 'license', licenseName='MIT', licenseURL='https://opensource.org/license/mit')
    element(header, 'description', 'Approximate single-engine trainer model for OneClickPilot')
    metrics = element(aircraft, 'metrics')
    values(metrics, {'wingarea': 174, 'wingspan': 36, 'chord': 4.83, 'htailarea': 23, 'htailarm': 14,
                     'vtailarea': 16, 'vtailarm': 13})
    location(metrics, 'AERORP', (0, 0, 0))
    location(metrics, 'EYEPOINT', (-12, -14, 31))
    location(metrics, 'VRP', (0, 0, 0))
    mass = element(aircraft, 'mass_balance')
    values(mass, {'ixx': 1050, 'iyy': 1450, 'izz': 2280, 'emptywt': 1680})
    location(mass, 'CG', (0, 0, 0))
    ground = element(aircraft, 'ground_reactions')
    for name, xyz, brake, spring, steer in (
        ('Nose', (-49, 0, -54), 'NONE', 2100, 10),
        ('Left', (10, -52, -54), 'LEFT', 3900, 0),
        ('Right', (10, 52, -54), 'RIGHT', 3900, 0),
    ):
        contact = element(ground, 'contact', type='BOGEY', name=name)
        location(contact, 'CONTACT', xyz)
        values(contact, {'static_friction': .79, 'dynamic_friction': .52, 'rolling_friction': .021,
                         'spring_coeff': spring, 'damping_coeff': 620, 'max_steer': steer,
                         'brake_group': brake, 'retractable': 0})

    propulsion = element(aircraft, 'propulsion')
    engine = element(propulsion, 'engine', file='practice_piston')
    values(engine, {'feed': 0})
    element(engine, 'feed', 1)
    thruster = element(engine, 'thruster', file='practice_prop')
    location(thruster, 'THRUSTER', (-66, 0, 0))
    orient = element(thruster, 'orient', unit='DEG')
    values(orient, {'roll': 0, 'pitch': 0, 'yaw': 0})
    values(thruster, {'sense': 1, 'p_factor': 2})
    for index, side in enumerate((-1, 1)):
        tank = element(propulsion, 'tank', type='FUEL', number=str(index))
        location(tank, 'TANK', (0, side * 80, 14))
        values(tank, {'capacity': 126, 'contents': 126, 'priority': 1})

    controls = element(aircraft, 'flight_control', name='Practice controls')
    channel = element(controls, 'channel', name='Flaps')
    flap = element(channel, 'kinematic', name='Flap actuator')
    element(flap, 'input', 'fcs/flap-cmd-norm')
    traverse = element(flap, 'traverse')
    for position, seconds in ((0, 0), (10, 1.8), (20, 1.2), (30, 1.2)):
        values(element(traverse, 'setting'), {'position': position, 'time': seconds})
    element(flap, 'output', 'fcs/flap-pos-deg')
    normalize = element(channel, 'pure_gain', name='Flap fraction')
    values(normalize, {'input': 'fcs/flap-pos-deg', 'gain': 1 / 30, 'output': 'fcs/flap-pos-norm'})

    aero = element(aircraft, 'aerodynamics')
    for axis, terms in {
        'LIFT': [('Wing lift', [('aero/alpha-rad', [(-.6, -.55), (-.28, -.85), (-.06, 0), (0, .3),
                                                   (.2, 1.28), (.28, 1.4), (.43, .82), (.8, .35)])]),
                 ('Flap lift', [.38, 'fcs/flap-pos-norm'])],
        'DRAG': [('Parasite drag', [.034]),
                 ('Induced drag', [('aero/alpha-rad', [(-.6, .28), (-.3, .06), (-.06, 0), (0, .005),
                                                      (.15, .06), (.28, .11), (.5, .34), (.8, .75)])]),
                 ('Flap drag', [.085, 'fcs/flap-pos-norm'])],
        'SIDE': [('Side slip', [-.34, 'aero/beta-rad']), ('Rudder side force', [-.06, 'fcs/rudder-cmd-norm'])],
        'ROLL': [('Dihedral', [-.079, 'aero/beta-rad']), ('Roll damping', [-.43, 'aero/bi2vel', 'velocities/p-aero-rad_sec']),
                 ('Aileron authority', [.043, 'fcs/aileron-cmd-norm'])],
        'PITCH': [('Pitch trim', [.035]), ('Pitch stability', [-.92, 'aero/alpha-rad']),
                  ('Pitch damping', [-9, 'aero/ci2vel', 'velocities/q-aero-rad_sec']),
                  ('Elevator authority', [-.34, 'fcs/elevator-cmd-norm']),
                  ('Elevator trim', [-.22, 'fcs/pitch-trim-cmd-norm']),
                  ('Flap moment', [-.04, 'fcs/flap-pos-norm'])],
        'YAW': [('Directional stability', [.085, 'aero/beta-rad']),
                ('Yaw damping', [-.13, 'aero/bi2vel', 'velocities/r-aero-rad_sec']),
                ('Rudder authority', [.035, 'fcs/rudder-cmd-norm'])],
    }.items():
        axis_node = element(aero, 'axis', name=axis)
        for name, factors in terms:
            product = element(element(axis_node, 'function', name='aero/practice/' + name.lower().replace(' ', '-')), 'product')
            element(product, 'property', 'aero/qbar-area')
            if axis in ('ROLL', 'YAW', 'PITCH'):
                element(product, 'property', 'metrics/cbarw-ft' if axis == 'PITCH' else 'metrics/bw-ft')
            for factor in factors:
                if isinstance(factor, tuple):
                    table(product, *factor)
                else:
                    element(product, 'property' if isinstance(factor, str) else 'value', factor)

    piston = ET.Element('piston_engine', name='Practice 320 cubic inch piston')
    values(piston, {'displacement': 320, 'maxhp': 160, 'cycles': 4, 'idlerpm': 700, 'maxrpm': 2700,
                    'compression-ratio': 8.5, 'minmp': 10, 'maxmp': 29.5})
    prop = ET.Element('propeller', name='Practice two-blade fixed pitch', version='1.01')
    values(prop, {'ixx': 1.3, 'numblades': 2, 'minpitch': 22, 'maxpitch': 22})
    element(prop, 'diameter', 75, unit='IN')
    table(prop, 'J', [(-.2, .125), (0, .11), (.3, .086), (.6, .05), (.85, .018), (1.1, -.013),
                      (1.5, -.04)], 'C_THRUST')
    table(prop, 'J', [(-.2, .07), (0, .058), (.3, .05), (.6, .042), (.85, .032), (1.1, .018),
                      (1.5, -.015)], 'C_POWER')
    write(root, 'aircraft/ocp172p/ocp172p.xml', aircraft)
    write(root, 'engine/practice_piston.xml', piston)
    write(root, 'engine/practice_prop.xml', prop)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    generate(parser.parse_args().destination)
