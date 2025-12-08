void main()
{
	vec3 normal, lightDir, viewVector, halfVector;
	vec4 diffuse, ambient, globalAmbient, specular = vec4(0.0);
	float NdotL, NdotHV;
	
	// trasformazione della normale nel sistema di rif. dell’osservatore e normalizzazione 

	normal = normalize(gl_NormalMatrix * gl_Normal);
	
	// normalizzazione della direzione della luce 	

	lightDir = normalize(vec3(gl_LightSource[0].position));
	
	// calcolo del coseno, assicurandosi di normalizzare il prodotto scalare tra 0 ed 1

	NdotL = max(dot(normal, lightDir), 0.0);
	
	// calcolo del termine di riflessione diffusa 

	diffuse = gl_FrontMaterial.diffuse * gl_LightSource[0].diffuse;

	// calcolo della luce ambiente 

	ambient = gl_FrontMaterial.ambient * gl_LightSource[0].ambient;
	globalAmbient = gl_LightModel.ambient * gl_FrontMaterial.ambient;

	// calcolo del termine di riflessione speculare se NdotL è maggiore di zero

	if (NdotL > 0.0) {

		NdotHV = max(dot(normal, normalize(gl_LightSource[0].halfVector.xyz)),0.0);
		specular = gl_FrontMaterial.specular * gl_LightSource[0].specular * pow(NdotHV,gl_FrontMaterial.shininess);
	}



	gl_FrontColor =  NdotL * diffuse + globalAmbient + ambient + specular; 

	gl_Position = ftransform();
} 
