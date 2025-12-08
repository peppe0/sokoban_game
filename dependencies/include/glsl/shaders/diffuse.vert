void main()
{
	vec3 normal, lightDir;
	vec4 diffuse = vec4(0.0);
	float NdotL;
	
	// trasformazione della normale nel sistema di rif. dell’osservatore e normalizzazione 

	normal = normalize(gl_NormalMatrix * gl_Normal);
	
	// normalizzazione della direzione della luce 	

	lightDir = normalize(vec3(gl_LightSource[0].position));
	
	// calcolo del coseno, assicurandosi di normalizzare il prodotto scalare tra 0 ed 1

	NdotL = max(dot(normal, lightDir), 0.0);
	
	// calcolo del termine di riflessione diffusa 

	diffuse = gl_FrontMaterial.diffuse * gl_LightSource[0].diffuse;

	gl_FrontColor = NdotL * diffuse;
	
	gl_Position = ftransform();
} 
